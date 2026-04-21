# Raspberry Pi Motor Control System

The embedded software that runs on a Raspberry Pi 4 to drive a DC motor over hardware PWM, measure speed via IR tachometer sensors, and accept real-time commands from an Android app over Bluetooth RFCOMM. A closed-loop feedback controller written in ARM assembly automatically adjusts motor power to hold a target RPM in the Maintain and Synced operating modes.

This is the counterpart to the Android Motor Controller App. The Android app serves as the user interface; this code is the real-time control backend.

## Repository Context

```
project-root/
├── android-app/
│   ├── activity_main.xml       # UI layout
│   ├── MainActivity.kt         # App logic and Bluetooth client
│   └── README.md
├── motor-control/              ← You are here
│   ├── bt_motor_control.c      # Main control program (Bluetooth server, GPIO, sensor polling)
│   ├── feedback.S              # ARM assembly adaptive proportional controller
│   ├── bluetooth-agent.service # systemd unit for auto-accepting BT pairings
│   └── README.md
```

## Source Files

**`bt_motor_control.c`** — The main program. It initializes the BCM2835 GPIO and PWM hardware, opens a Bluetooth RFCOMM server socket on channel 1, and enters a two-layer loop structure: an outer loop that blocks on `accept()` waiting for the Android app to connect, and an inner high-speed loop that polls IR sensors, computes RPM, runs the feedback controller, and checks for incoming Bluetooth commands using non-blocking `select()`. Telemetry (`MEASURED_RPM` and `EXTERNAL_RPM`) is transmitted back to the app once per second.

**`feedback.S`** — A 32-bit ARM (AArch32) assembly function implementing an adaptive proportional controller. It is called from the C program once per second during Maintain or Synced mode. Rather than using a fixed proportional gain, it dynamically selects a bit-shift divisor based on the magnitude of the RPM error: large errors produce aggressive corrections (divide by 8), while small errors below 100 RPM receive gentle adjustments (divide by 64) to create a stable deadband and prevent oscillation. An additional dampening shift is applied when the target speed is below 1000 RPM to avoid over-torquing at low speeds. The output PWM is clamped to the range 0–1000 before returning.

**`bluetooth-agent.service`** — A systemd unit that launches `bt-agent` in `NoInputNoOutput` mode and enables Bluetooth discoverability at boot. This allows the Pi to accept pairing requests from the Android app without requiring a keyboard or display attached to the Pi.

## Operating Modes

The system supports three modes, selectable from the Android app. The mode determines how `current_pwm` is updated each control cycle.

**Manual** — The app sends `PWM:<value>` commands directly. The C program writes that value to the hardware PWM channel with no feedback. The feedback controller is not invoked.

**Maintain** — The app sends a `TARGET_RPM:<value>`. Each second, the C program reads the controlled motor's RPM from the IR tachometer and calls `calculate_feedback_pwm()` in `feedback.S`, which returns an adjusted PWM value. This creates a closed loop: if the motor is too slow, power increases; if too fast, power decreases.

**Synced** — Similar to Maintain, but the target RPM is not set by the user. Instead, the C program reads a second IR sensor attached to an external "master" motor and continuously overwrites `target_rpm` with the external motor's measured speed. The same assembly feedback controller then drives the controlled motor to match.

## Hardware Configuration

All pin assignments use BCM numbering and are defined at the top of `bt_motor_control.c`.

| Pin  | BCM GPIO | Function                                      |
|------|----------|-----------------------------------------------|
| PWM  | 18       | Hardware PWM output (Channel 0, ALT5 function) |
| DIR1 | 23       | H-bridge direction input A                     |
| DIR2 | 24       | H-bridge direction input B                     |
| IR   | 25       | IR tachometer sensor (controlled motor)         |
| EXT_IR | 19     | IR tachometer sensor (external motor)           |

The PWM is configured with a clock divider of 16 and a range of 1024, giving a resolution of 1024 discrete power levels. The app-facing protocol uses a logical range of 0–1000; values are clamped in `process_command()`.

IR sensors are read as digital GPIO inputs with internal pull-up resistors disabled (`PUD_OFF`). RPM is calculated by counting falling-edge transitions over a one-second window, assuming a 3-slot encoder disc: `RPM = (pulse_count × 60) / 3`.

## Communication Protocol

Identical to the protocol documented in the Android app README. All messages are newline-delimited plaintext over Bluetooth RFCOMM (SPP UUID `00001101-0000-1000-8000-00805F9B34FB`, channel 1).

The C program parses incoming commands in `process_command()` using `strtok()` to split on `\r\n` delimiters, then matches each token with `strncmp()`. Telemetry is transmitted via `write()` on the client socket; a failed write is treated as a disconnection event.

## Feedback Controller Details

`calculate_feedback_pwm` in `feedback.S` follows the ARM calling convention (AAPCS): inputs arrive in `r0` (target RPM), `r1` (current RPM), and `r2` (current PWM); the adjusted PWM is returned in `r0`.

The adaptive gain schedule is:

| Absolute Error (RPM) | Bit Shift | Effective Divisor | Behavior              |
|-----------------------|-----------|-------------------|-----------------------|
| ≥ 600                 | 3         | 8                 | Aggressive correction |
| 300–599               | 4         | 16                | Moderate correction   |
| 100–299               | 5         | 32                | Gentle correction     |
| < 100                 | 6         | 64                | Deadband (fine trim)  |

If the target RPM is below 1000, one additional shift is applied (doubling the divisor) to dampen corrections at low speeds where the same PWM delta produces a proportionally larger torque change.

A kickstart mechanism in the C code sets `current_pwm` to 200 when transitioning from a dead stop with a nonzero target, providing enough initial torque to overcome static friction before the feedback loop takes over.

## Build and Deployment

### Dependencies

- **bcm2835 library** — C library for Raspberry Pi GPIO and PWM access. Install from [airspayce.com/mikem/bcm2835](http://www.airspayce.com/mikem/bcm2835/).
- **BlueZ development headers** — `libbluetooth-dev` package for RFCOMM socket support.
- **ARM assembler** — GCC cross-compiler or native `gcc` on the Pi (supports 32-bit ARM assembly via `as`).
- **bt-agent** — Part of the `bluez-tools` package, used by the systemd service.

### Compilation

```bash
# Assemble the feedback controller
as -o feedback.o feedback.S

# Compile and link
gcc -o bt_motor_control bt_motor_control.c feedback.o -lbcm2835 -lbluetooth
```

The binary must be run as root (or with appropriate GPIO permissions) since `bcm2835_init()` requires access to `/dev/mem`.

```bash
sudo ./bt_motor_control
```

### Bluetooth Agent Setup

Copy the service file and enable it so pairing works headlessly on boot:

```bash
sudo cp bluetooth-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable bluetooth-agent.service
sudo systemctl start bluetooth-agent.service
```

Verify the Pi is discoverable:

```bash
bluetoothctl show | grep Discoverable
```

## Safety Behavior

The system includes several fail-safe mechanisms:

- **Disconnect shutdown**: If `write()` fails when sending telemetry or `select()` detects a zero-byte read, the inner loop breaks, the motor PWM is immediately set to 0, and all state variables are reset to defaults before the program re-enters the connection-wait state.
- **PWM clamping**: Both the C command parser and the assembly feedback controller independently clamp PWM output to 0–1000, preventing out-of-range values from reaching the hardware.
- **Zero-target guard**: In Maintain and Synced modes, if `target_rpm` is 0, the feedback loop is bypassed entirely and PWM is forced to 0. This prevents the controller from accumulating error against an undefined target.
- **Mode-gated commands**: `PWM:<value>` commands are ignored unless the system is in Manual mode. `TARGET_RPM:<value>` commands are ignored in Synced mode (where the external sensor dictates the target).

## Known Limitations

- **Polling-based RPM measurement**: The inner loop uses a tight poll with a 100 µs delay between GPIO reads. At very high RPM, pulses shorter than the polling interval may be missed. An interrupt-driven edge counter would be more robust but is not supported by the bcm2835 library's userspace model.
- **1-second feedback interval**: The feedback controller runs once per second. This limits the system's response bandwidth and means transient load disturbances shorter than one second are not corrected until the next cycle.
- **No integral or derivative terms**: The assembly controller is purely proportional (with adaptive gain). Steady-state error can persist when the deadband divisor is too large relative to the remaining RPM offset. Adding an integral accumulator would eliminate this but increases complexity and risk of windup.
- **Single-client Bluetooth**: The RFCOMM server accepts one connection at a time. A second device attempting to connect will block until the first disconnects.
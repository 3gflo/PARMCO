# 🍓 PARMCO: Motor Control Software

[![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi)](https://www.raspberrypi.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Assembly](https://img.shields.io/badge/Assembly-000000?style=for-the-badge&logo=assembly)](https://en.wikipedia.org/wiki/Assembly_language)

This directory contains all Raspberry Pi software for PARMCO. The codebase is written in C and ARM Assembly for maximum performance and minimal latency, interfacing directly with GPIO hardware via the `bcm2835` library.

For hardware schematics and a full system overview, see the [top-level README](../README.md).

---

## 📂 Folder Contents

| File | Description |
| :--- | :--- |
| **`basic_motor_control.c`** | Standalone hardware test program. Generates hardware PWM, reads keyboard input non-blockingly (`kbhit`), and polls the IR sensor at ~1000 Hz for real-time RPM calculation. |
| **`motor_server.c`** | Bluetooth RFCOMM server. Binds to Channel 1, accepts the Android app connection, parses incoming commands, runs the main control loop, and streams telemetry back at ~4Hz. |
| **`control.S`** | ARM Assembly implementation of the `adjust_pwm` proportional controller. Offloads the closed-loop error calculation from C for maximum timing precision. |
| **`Makefile`** | Compiles C and Assembly sources into a single executable and runs it with the required root privileges. |
| **`image_a9dc87.jpg`** | Hardware schematic showing the L293D H-Bridge, IRFZ34N MOSFET indicator circuit, and power isolation. |

---

## ⚙️ GPIO Pin Mappings

The software interfaces with hardware through the `bcm2835` library using memory-mapped GPIO access, bypassing Linux kernel overhead for precise microsecond timing.

> **Important:** There is an intentional discrepancy between the test schematic (`image_a9dc87.jpg`) and the final software pin definitions. Wire the physical hardware to the **Software (BCM)** column below.

| Component Function | Macro Name | BCM GPIO (Software) | BCM GPIO (Schematic) |
| :--- | :--- | :---: | :---: |
| PWM Speed Control → L293D EN1 | `PWM_PIN` | **18** |
| H-Bridge Forward → L293D IN1 | `DIR1_PIN` | **23** |
| H-Bridge Reverse → L293D IN2 | `DIR2_PIN` | **24** |
| IR Sensor Pulse Input | `IR_PIN` | **25** |

---

## 📡 RPM Encoding Logic

RPM is measured using a standard 3-pin IR Obstacle Avoidance Sensor aimed at the propeller.

`basic_motor_control.c` polls GPIO 25 at ~1000 Hz. Each **falling edge** (HIGH → LOW transition) represents one blade passing the sensor. Since the propeller has 3 blades, one full rotation produces 3 pulses. After accumulating pulses over a 1-second window:

$$RPM = (\text{Pulse Count} \times 60) / 3$$

This polling approach (rather than hardware interrupts) is used in the test script for simplicity. `motor_server.c` integrates this count into the main telemetry loop.

---

## 🧠 Assembly Proportional Controller (`control.S`)

The closed-loop speed controller is implemented in ARM Assembly to ensure the error correction calculation adds negligible latency to the control loop.

### C Signature
```c
int adjust_pwm(int current_rpm, int target_rpm, int current_pwm, int pwm_range);
```
Arguments map to ARM registers `r0`–`r3`. The function returns the corrected PWM value in `r0`.

### Algorithm

1. **Error Calculation**
   ```
   error = target_rpm - current_rpm
   ```

2. **Dynamic Gain** — gain is selected based on the target RPM tier to balance responsiveness against oscillation:

   | Condition | Shift | Effective Divisor | Behaviour |
   | :--- | :--- | :---: | :--- |
   | `target_rpm < 1500` | `asr #5` | ÷ 32 | Gentle — prevents oscillation at low speeds |
   | `target_rpm >= 1500` | `asr #4` | ÷ 16 | Aggressive — delivers torque to reach high speeds quickly |

3. **PWM Clamping**
   ```
   new_pwm = clamp(current_pwm + gain_adjusted_error, 0, pwm_range)
   ```
   `pwm_range` is 1024. The clamp prevents the output from exceeding hardware limits or going negative.

---

## 🛠️ Compilation & Usage

### Prerequisites

Install the `BlueZ` development headers on your Raspberry Pi:
```bash
sudo apt-get update
sudo apt-get install libbluetooth-dev
```

The `bcm2835` library must be compiled and installed from source:
```
http://www.airspayce.com/mikem/bcm2835/
```

### Build with Makefile
```bash
# Build all sources (C + Assembly) into a single executable
make

# Run the server (root required for GPIO and Bluetooth access)
sudo ./motor_server
```

### Standalone Hardware Test (no Bluetooth)
```bash
# Compile and run the local test script directly
gcc -o basic_motor_control basic_motor_control.c -lbcm2835
sudo ./basic_motor_control
```

Use keyboard input in the terminal to test PWM ramp-up, direction switching, and live RPM readout without needing the Android app connected.
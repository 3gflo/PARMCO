# Android Motor Controller App

An Android application that provides real-time wireless control of a DC motor connected to a Raspberry Pi 4 via Bluetooth RFCOMM. The app supports three operating modes—Manual, Maintain, and Synced—and displays live telemetry from the Pi's sensor feedback loop.

This app is one component of a larger embedded motor control system. The Raspberry Pi runs a C program (`bt_motor_control.c`) that interfaces with hardware PWM, IR tachometer sensors, and an ARM assembly PID-style feedback controller (`feedback.S`). This Android app serves as the human interface to that system.

## Repository Context

```
project-root/
├── android-app/            ← You are here
│   ├── activity_main.xml       # UI layout (ScrollView with control sections)
│   ├── MainActivity.kt         # Application logic and Bluetooth communication
│   └── README.md
├── bt_motor_control.c          # Pi-side C program (Bluetooth server, GPIO, control loop)
├── feedback.S                  # ARM assembly adaptive proportional controller
└── bluetooth-agent.service     # systemd unit for auto-accepting BT pairings on the Pi
```

## Features

- **Bluetooth RFCOMM Connection** — Scans for nearby devices, lists paired and discovered devices in a selection dialog, and establishes a serial socket connection to the Pi.
- **Three Operating Modes**
  - **Manual**: Directly sets motor PWM (power) via increment/decrement buttons in steps of 50 (range 0–1000).
  - **Maintain**: Sends a target RPM to the Pi, which runs a closed-loop feedback controller (written in ARM assembly) to hold that speed automatically.
  - **Synced**: The Pi reads an external motor's RPM via a second IR sensor and dynamically matches the controlled motor's speed to it. No user input is required beyond selecting the mode.
- **Live Telemetry Display** — Receives and parses `MEASURED_RPM` and `EXTERNAL_RPM` data from the Pi once per second, updating the UI in real time.
- **Direction Control** — A toggle switch sends `DIR:FORWARD` or `DIR:REVERSE` commands to flip the H-bridge driver polarity.
- **State Synchronization** — On connection, the app transmits its full UI state (mode, PWM, direction, target RPM) to the Pi so both sides start in agreement.

## Communication Protocol

All communication uses newline-delimited plaintext strings over Bluetooth RFCOMM (SPP UUID `00001101-0000-1000-8000-00805F9B34FB`).

### App → Pi (Commands)

| Command              | Description                                  |
|----------------------|----------------------------------------------|
| `STATE:START`        | Enable motor output                          |
| `STATE:STOP`         | Disable motor output (PWM forced to 0)       |
| `DIR:FORWARD`        | Set H-bridge to forward polarity             |
| `DIR:REVERSE`        | Set H-bridge to reverse polarity             |
| `MODE:MANUAL`        | Switch to manual PWM control                 |
| `MODE:MAINTAIN`      | Switch to closed-loop speed hold             |
| `MODE:SYNCED`        | Switch to external motor speed tracking      |
| `PWM:<value>`        | Set raw PWM duty cycle (0–1000, Manual only) |
| `TARGET_RPM:<value>` | Set desired RPM (Maintain mode)              |

### Pi → App (Telemetry)

| Message                  | Description                              |
|--------------------------|------------------------------------------|
| `MEASURED_RPM:<value>`   | Controlled motor speed from IR tachometer |
| `EXTERNAL_RPM:<value>`   | External motor speed from second IR sensor |

## Architecture

The app follows a single-Activity architecture. `MainActivity.kt` handles all UI binding, state management, and Bluetooth I/O.

**Threading model**: Bluetooth socket connection and the telemetry listener run on a dedicated background thread spawned in `connectToDevice()`. All UI updates from that thread are dispatched back to the main thread via `runOnUiThread{}`. The high-speed control loop and sensor polling happen entirely on the Pi side; the app only sends commands and receives telemetry at ~1 Hz.

**Connection lifecycle**: The app uses `select()`-style non-blocking reads on the Pi side, not on the Android side. The Android listener thread blocks on `inputStream.read()` and catches `IOException` to detect disconnection, at which point the UI reverts to a "Disconnected" state.

## Build Requirements

- **Min SDK**: 21 (Android 5.0 Lollipop)
- **Target SDK**: 34 (Android 14) or higher recommended
- **Language**: Kotlin
- **Dependencies**: Android SDK only (no external libraries)
- **Permissions** (declared in `AndroidManifest.xml`):
  - `BLUETOOTH`, `BLUETOOTH_ADMIN` — legacy Bluetooth access
  - `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN` — required on Android 12+ (API 31)
  - `ACCESS_FINE_LOCATION` — required for BT discovery on Android 11 and below

## Setup and Deployment

1. Open the project in Android Studio.
2. Ensure `AndroidManifest.xml` declares the Bluetooth permissions listed above.
3. Verify that `activity_main.xml` is located at `res/layout/activity_main.xml` and that the package name in `MainActivity.kt` matches your project's `applicationId` in `build.gradle`.
4. Build and install to a physical Android device (Bluetooth is not available on the emulator).
5. Pair with the Raspberry Pi through the in-app device selection dialog or through Android system settings beforehand.

## Known Limitations

- **No persistent reconnection**: If the Bluetooth connection drops, the user must manually tap "Connect to Pi" again. There is no automatic reconnection attempt on the Android side (the Pi side does re-listen for new connections automatically).
- **Single-thread telemetry parsing**: The `listenForData()` loop uses a fixed 1024-byte buffer. If the Pi sends data faster than the app reads it, multiple messages may arrive concatenated in one read. This is handled by splitting on newline characters, but partial messages split across two reads are not explicitly reassembled.
- **No input validation on Target RPM**: The `EditText` field accepts any integer. There is no upper-bound check on the app side (the Pi's feedback loop will clamp PWM output to 0–1000, but the user could enter an unachievable RPM).
- **Bluetooth Classic only**: The app uses RFCOMM (Bluetooth Classic), not BLE. The target device must support the SPP profile.

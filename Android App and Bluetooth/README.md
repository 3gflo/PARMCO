# 📱 PARMCO: Android Application & Bluetooth Subsystem

[![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com/)
[![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)

This directory contains the Android application for PARMCO. The app provides a complete wireless interface for motor control and live telemetry, communicating with the Raspberry Pi over a persistent **Bluetooth Serial Port Profile (SPP)** connection via RFCOMM.

For hardware details and a system overview, see the [top-level README](../README.md). For the Pi-side server code, see [`motor-control/`](../motor-control/README.md).

---

## 📂 Folder Contents

| File | Description |
| :--- | :--- |
| **`MainActivity.kt`** | Core Kotlin application logic. Handles UI interactions, Bluetooth device discovery, socket lifecycle, and threaded data transmission/reception. |
| **`activity_main.xml`** | Android XML layout defining the full user interface. |

> **Note:** `motor_server.c` (the Pi-side RFCOMM server) lives in [`motor-control/`](../motor-control/) and is documented there.

---

## 📡 Bluetooth Architecture

The connection uses classic Bluetooth (BR/EDR) RFCOMM, which emulates a reliable serial cable link between the phone and the Pi.

**Server (Raspberry Pi):** The `BlueZ`-based C server binds a listening socket to **Channel 1**. It uses `select()` with a 250ms timeout to run a non-blocking read/write loop — processing any incoming commands while streaming telemetry at ~4Hz regardless of user input.

**Client (Android):** The app connects using the standard SPP UUID: `00001101-0000-1000-8000-00805F9B34FB`. All inbound telemetry is processed on a dedicated background thread, with `runOnUiThread` used to safely push updates to the UI without blocking.

---

## 🗣️ Communication Protocol

All messages are plain ASCII strings terminated by a newline (`\n` or `\r\n`). This is the canonical protocol definition for the entire PARMCO system.

### 📱 Android → 🍓 Raspberry Pi (Commands)

| Category | Command String | Description |
| :--- | :--- | :--- |
| **Power State** | `STATE:START` | Energizes the motor system. |
| | `STATE:STOP` | Immediately cuts power (target RPM → 0). |
| **Direction** | `DIR:FORWARD` | Sets H-Bridge IN1 HIGH / IN2 LOW (clockwise). |
| | `DIR:REVERSE` | Sets H-Bridge IN1 LOW / IN2 HIGH (counter-clockwise). |
| **Control Mode** | `MODE:MANUAL` | Open-loop: PWM maps directly to target value. |
| | `MODE:MAINTAIN` | Closed-loop: Assembly PID holds a specific RPM. |
| | `MODE:SYNCED` | Closed-loop: Matches RPM to a secondary IR sensor. |
| **Throttle** | `RPM:<integer>` | Sets target speed in RPM (e.g., `RPM:1500`). |

### 🍓 Raspberry Pi → 📱 Android (Telemetry)

| Message Format | Description |
| :--- | :--- |
| `MEASURED_RPM:<integer>` | Live RPM calculated from IR sensor pulse counting, streamed at ~4Hz. |

---

## 📱 Application Interface

The app is designed for quick, accessible control in a lab or testing environment.

**Device Discovery:** A built-in scanner lists both paired and newly discovered Bluetooth devices. The app handles the initial pairing bond handshake automatically when connecting to a new Pi.

**Directional Control:** A toggle switch for instant CW/CCW switching, sending `DIR:FORWARD` or `DIR:REVERSE` on change.

**Mode Selection:** A `RadioGroup` ensures only one mode (`MANUAL`, `MAINTAIN`, `SYNCED`) is active at a time, preventing contradictory state commands.

**Speed Tuning:** Target RPM can be incremented/decremented in steps of 25 via +/− buttons, or typed directly via the numeric keypad. Both paths dispatch a `RPM:<value>` command.

**Live Dashboard:** The measured RPM readout is displayed in bold red, making it easy to compare the actual physical state against the target at a glance.

---

## 🛠️ Setup & Requirements

### Android Permissions

The required permissions vary by Android version:

| Android Version | Required Permissions |
| :--- | :--- |
| **12+ (API 31+)** | `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN` |
| **11 and below** | `ACCESS_FINE_LOCATION` (needed to scan hardware MAC addresses) |

### Build & Install

Open the `android-app/` directory in **Android Studio**, let Gradle sync, then build and install on a Bluetooth-capable device.

### Testing the Server Without Hardware

You can validate the Android connection and UI behaviour without any motor hardware attached by running the server in isolation on the Pi:

```bash
# Compile the Bluetooth server only (no bcm2835 dependency)
gcc -o motor_server motor_server.c -lbluetooth

# Run the server
sudo ./motor_server
```

The server will accept the app's connection and respond to all commands, making it straightforward to test mode switching, RPM input, and telemetry display before the physical circuit is wired.
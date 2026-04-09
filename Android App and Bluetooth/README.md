# 📱 PARMCO: Android Application & Bluetooth Subsystem

[![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com/)
[![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)

This directory contains the mobile interface and the communication backend for the **Phone APP RP4 Motor Control (PARMCO)** project. It includes the Kotlin source code for the Android application and the C-based Bluetooth server that runs on the Raspberry Pi 4.

The system relies on the **Bluetooth Serial Port Profile (SPP)** via RFCOMM to maintain a persistent, bidirectional communication link between the user's phone and the hardware logic.

---

## 📂 Folder Contents

* **`MainActivity.kt`**: The core Kotlin logic for the Android application. Handles UI interactions, Bluetooth device discovery, socket connections, and threaded data transmission.
* **`activity_main.xml`**: The Android XML layout file defining the user interface.
* **`motor_server.c`**: The C script that runs on the Raspberry Pi. It binds to an RFCOMM socket, acts as a server to accept the Android connection, parses incoming commands, and streams telemetry data back to the phone.

---

## 📡 Bluetooth Architecture

The communication bridge is built on classic Bluetooth (BR/EDR) using the RFCOMM protocol, which emulates a reliable serial cable connection.

* **Server (Raspberry Pi):** Utilizes the `BlueZ` library to bind a listening socket to **Channel 1**. It uses `select()` with a 250ms timeout to create a non-blocking read/write loop, allowing it to process incoming commands while steadily streaming telemetry at ~4Hz.
* **Client (Android):** Connects to the Pi using the standard SPP UUID: `00001101-0000-1000-8000-00805F9B34FB`. It handles incoming telemetry on a dedicated background thread to prevent UI freezing, utilizing `runOnUiThread` to safely update the display.

---

## 🗣️ Communication Protocol

The devices communicate using plain ASCII text terminated by newline characters (`\n` or `\r\n`).

### 📱 Android ➔ 🍓 Raspberry Pi (Commands)
The app sends the following strictly formatted strings to control the motor state:

| Category | Command String | Description |
| :--- | :--- | :--- |
| **Power State** | `STATE:START` | Energizes the motor system. |
| | `STATE:STOP` | Immediately cuts power (Target RPM = 0). |
| **Direction** | `DIR:FORWARD` | Sets the H-Bridge for clockwise rotation. |
| | `DIR:REVERSE` | Sets the H-Bridge for counter-clockwise rotation. |
| **Control Mode**| `MODE:MANUAL` | Open-loop control. PWM maps directly to target. |
| | `MODE:MAINTAIN` | Closed-loop PID control to hold a specific RPM. |
| | `MODE:SYNCED` | Closed-loop control mimicking a secondary IR sensor. |
| **Throttle** | `RPM:<integer>` | Sets the target speed (e.g., `RPM:1500`). |

### 🍓 Raspberry Pi ➔ 📱 Android (Telemetry)
The Pi streams data back to the app, which is parsed and displayed in the Live Telemetry section.

| Message Format | Description |
| :--- | :--- |
| `MEASURED_RPM:<integer>` | The actual RPM calculated from the physical IR sensor. |

---

## 📱 Application Interface

The Android app is designed for quick, accessible control in a lab or testing environment.

* **Device Discovery:** A built-in scanner lists both paired and newly discovered Bluetooth devices. It handles the initial pairing bond request if connecting to a new Pi.
* **Intuitive Controls:** Uses a toggle switch for quick directional changes and an active RadioGroup to prevent contradictory mode selections.
* **Granular Tuning:** Target RPM can be stepped up/down in increments of 25 using buttons, or explicitly set via the numeric keypad.
* **Live Dashboard:** Highlights the "Measured RPM" in bold red to easily compare the system's intended target versus its actual physical state.

---

## 🛠️ Setup & Requirements

### Android Permissions
The app is configured to support modern Android permission models. Depending on the device's Android version, it requires:
* **Android 12+ (API 31+):** `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN`
* **Android 11 and lower:** `ACCESS_FINE_LOCATION` (Required by Android to scan for hardware MAC addresses).

### Testing the Server Locally
You can compile and run the Bluetooth server on the Pi independently of the hardware controls to test the Android connection and UI state changes:

```bash
# Compile the BlueZ server
gcc -o motor_server motor_server.c -lbluetooth

# Run the server
sudo ./motor_server
# 📱 PARMCO: Android Application

[![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com/)
[![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)

This directory contains the Android frontend application for the **Phone APP RP4 Motor Control (PARMCO)** project. The app provides a wireless interface for motor control and live telemetry, communicating over a persistent **Bluetooth Serial Port Profile (SPP)** connection via RFCOMM.

---

## 📂 Folder Contents

| File | Description |
| :--- | :--- |
| **`MainActivity.kt`** | **Core Application Logic.** Manages the Bluetooth lifecycle (discovery, bonding, and socket connection), handles UI thread synchronization, and processes bidirectional data streams. |
| **`activity_main.xml`** | **User Interface Layout.** A scrollable layout containing connection status indicators, motor toggle controls, mode selection (RadioGroups), and live telemetry displays. |

---

## 📡 Bluetooth Client Architecture

The application acts as a Bluetooth Client connecting to a remote hardware server (such as a Raspberry Pi). 

* **Connection Protocol:** The app utilizes classic Bluetooth (BR/EDR) and connects using the standard SPP UUID: `00001101-0000-1000-8000-00805F9B34FB`.
* **Thread Management:** To ensure a responsive UI, the app initiates the socket connection and the inbound telemetry listener on dedicated background threads. It uses `runOnUiThread` to safely update UI components when data is received.
* **Discovery & Bonding:** The app includes a built-in discovery mechanism to scan for nearby devices and supports system-level bonding (pairing) directly from the selection dialog.

---

## 🗣️ App Communication Protocol

The app communicates using plain ASCII strings terminated by a newline (`\n`). 

### Outbound Commands (App → Pi)
The following commands are dispatched based on user interaction:

| Category | Command String | Triggered By |
| :--- | :--- | :--- |
| **Power State** | `STATE:START` <br> `STATE:STOP` | "Start/Stop Motor" Button |
| **Direction** | `DIR:FORWARD` <br> `DIR:REVERSE` | "Reverse" Switch |
| **Control Mode** | `MODE:MANUAL` <br> `MODE:MAINTAIN` <br> `MODE:SYNCED` | Mode Selection Radio Group |
| **Throttle** | `RPM:<0-1000>` | "Increase/Decrease" Buttons (Steps of 50). |

### Inbound Telemetry (Pi → App)
The app's `listenForData()` loop parses incoming strings to update the dashboard:

| Message Format | Action |
| :--- | :--- |
| `MEASURED_RPM:<integer>` | Updates the **Measured RPM** text view with the real-time value from the motor sensors. |

---

## 🛠️ Setup & Requirements

### Android Permissions
The app handles dynamic permission requests. Depending on your Android version, the following are required:
* **Android 12+ (API 31+):** `BLUETOOTH_CONNECT` and `BLUETOOTH_SCAN`.
* **Android 11 & Below:** `ACCESS_FINE_LOCATION` (required by Android to access Bluetooth hardware identifiers).

### Build & Install
1. Open this project directory in **Android Studio**.
2. Connect a **physical Android device** (Bluetooth is not supported on standard emulators).
3. Build and deploy the APK to your device.
4. Use the **"Connect to Pi"** button to pair and establish the RFCOMM link.

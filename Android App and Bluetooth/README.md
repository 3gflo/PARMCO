# 📱 PARMCO: Android Application

[![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com/)
[![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)

This directory contains the Android frontend application for the **Phone APP RP4 Motor Control (PARMCO)** project. The app provides a wireless interface for motor control and live telemetry, communicating over a long-lived **Bluetooth Serial Port Profile (SPP)** connection via RFCOMM. Note that there is currently no automatic reconnect — if the socket drops, the user must tap **Connect to Pi** again.

---

## 📂 Folder Contents

| File | Description |
| :--- | :--- |
| **`MainActivity.kt`** | **Core Application Logic.** Manages the Bluetooth lifecycle (discovery, bonding, and socket connection), handles UI thread synchronization, and processes bidirectional data streams. |
| **`activity_main.xml`** | **User Interface Layout.** A vertical `ScrollView` containing (top to bottom): connection status text, Connect button, Start/Stop button paired with a Reverse switch, a single mode-selection `RadioGroup` (Manual / Maintain / Synced — defaults to Manual on launch), a manual speed panel with Decrease/Increase (±50) buttons, and a live telemetry section showing the locally-tracked **Intended PWM** (`tvIntendedPwm`) and the sensor-reported **Measured RPM** (`tvMeasuredRpm`, styled in bold red as the primary readout). |

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
| **Power State** | `STATE:START` <br> `STATE:STOP` | "Start/Stop Motor" Button (single toggle) |
| **Direction** | `DIR:FORWARD` <br> `DIR:REVERSE` | "Reverse" Switch (unchecked → FORWARD, checked → REVERSE) |
| **Control Mode** | `MODE:MANUAL` <br> `MODE:MAINTAIN` <br> `MODE:SYNCED` | Mode Selection Radio Group |
| **Throttle** | `RPM:<0-1000>` | "Increase/Decrease" Buttons (Steps of 50). **Only sent in MANUAL mode.** |

> ⚠️ **Cross-component note:** The `MODE:*` commands are currently **silently ignored by the Raspberry Pi server** — mode behavior is entirely client-side UI state. The commands are sent for forward compatibility, but selecting a mode today only changes what the app allows the user to do, not what the Pi does. See the Motor Control README for details on which commands the Pi actually acts on.

### Client-Side Mode Behavior
The three modes change what the app allows the user to do locally:

| Mode | +/- RPM Buttons | On Selection |
| :--- | :--- | :--- |
| **MANUAL** | Enabled | Sends `MODE:MANUAL`, then immediately re-sends current `RPM:<value>` to resync the Pi's PWM. |
| **MAINTAIN** | Disabled (greyed out) | Sends `MODE:MAINTAIN`. Intended for closed-loop speed hold (server-side logic not yet implemented). |
| **SYNCED** | Disabled (greyed out) | Sends `MODE:SYNCED`. Intended for external-sensor-driven control (server-side logic not yet implemented). |

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
4. Tap **"Connect to Pi"** to open the device selection dialog. Previously paired devices appear immediately; nearby unpaired devices populate as they are discovered.
5. **First-time pairing:** If you select an unpaired device, the app triggers an Android bond request and shows a toast instructing you to tap **Connect to Pi** a second time once pairing completes. This is a known two-tap UX quirk — subsequent launches connect in one tap since the device is already bonded.

### Connection Lifecycle
* The RFCOMM socket and the inbound `listenForData()` loop run on a dedicated background thread.
* If the socket drops (read `IOException`), the status text updates to **"Disconnected"** and the listener thread exits. There is no automatic reconnect — the user must tap **Connect to Pi** again.
* On `onDestroy()`, the app closes the socket and unregisters the device-discovery `BroadcastReceiver`.

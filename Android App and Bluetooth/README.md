# 📱 PARMCO: Android Application

[![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)](https://developer.android.com/)
[![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)

This directory contains the Android frontend application for the **Phone APP RP4 Motor Control (PARMCO)** project. The app provides a complete wireless interface for motor control and live telemetry, communicating over a persistent **Bluetooth Serial Port Profile (SPP)** connection via RFCOMM.

---

## 📂 Folder Contents

| File | Description |
| :--- | :--- |
| **`MainActivity.kt`** | Core Kotlin application logic. Handles UI interactions, Bluetooth device discovery, socket lifecycle, and threaded data transmission/reception. |
| **`activity_main.xml`** | Android XML layout defining the user interface, including connection status, telemetry displays, and motor control inputs. |

---

## 📡 Bluetooth Client Architecture

The application acts as a Bluetooth Client connecting to a remote hardware server. 

* **Connection Protocol:** The app utilizes classic Bluetooth (BR/EDR) and connects using the standard SPP UUID: `00001101-0000-1000-8000-00805F9B34FB`.
* **Thread Management:** All inbound telemetry is processed on a dedicated background thread to prevent UI freezing. The `runOnUiThread` method is utilized to safely push text updates to the screen when new data arrives.
* **Error Handling:** If the Bluetooth socket drops or throws an `IOException`, the app gracefully catches the error, updates the UI to "Disconnected," and safely closes the streams.

---

## 🗣️ App Communication Protocol

The app parses and transmits plain ASCII strings terminated by a newline (`\n`). 

### App Transmissions (Commands)
When the user interacts with the UI, the app dispatches the following strings over the Bluetooth socket:

| Category | Command String | Triggered By |
| :--- | :--- | :--- |
| **Power State** | `STATE:START` <br> `STATE:STOP` | "Start/Stop Motor" Button |
| **Direction** | `DIR:FORWARD` <br> `DIR:REVERSE` | "Direction: Forward/Reverse" Switch |
| **Control Mode** | `MODE:MANUAL` <br> `MODE:MAINTAIN` <br> `MODE:SYNCED` | Mode Selection Radio Group |
| **Throttle** | `RPM:<integer>` | "+50" and "-50" Buttons. <br>*(Note: In Manual mode, this value scales from 0 to 1000 to represent PWM duty cycle).* |

### App Receptions (Telemetry)
The app's input stream constantly listens for the following string format to update the Live Dashboard:

| Message Format | Action |
| :--- | :--- |
| `MEASURED_RPM:<integer>` | Parses the integer and updates the bold red "Measured RPM" text view. |

---

## 📱 Application Interface

The app is designed for quick, accessible control in a lab or testing environment.

* **Device Discovery:** A built-in scanner lists both paired and newly discovered Bluetooth devices. The app triggers standard Android pairing requests if connecting to a new device for the first time.
* **Safety & State:** A `RadioGroup` ensures only one mode (`MANUAL`, `MAINTAIN`, `SYNCED`) is active at a time, preventing contradictory commands from being sent.
* **Speed Tuning:** Target speed/PWM is safely bounded between 0 and 1000. It can be stepped up or down in increments of 50 using the large UI buttons.
* **Live Dashboard:** Provides a clear visual contrast between the *Target PWM* (intended output) and the *Measured RPM* (actual physical state) at a glance.

---

## 🛠️ Setup & Requirements

### Android Permissions

The app requests Bluetooth permissions dynamically. Depending on the target device's Android version, it requires:

| Android Version | Required Permissions |
| :--- | :--- |
| **12+ (API 31+)** | `BLUETOOTH_CONNECT`, `BLUETOOTH_SCAN` |
| **11 and below** | `ACCESS_FINE_LOCATION` (Required by the Android OS to scan for nearby hardware MAC addresses). |

### Build & Install

1. Open this directory in **Android Studio**.
2. Allow Gradle to sync the project dependencies.
3. Connect a physical Android device with Bluetooth capabilities (Emulators do not support Bluetooth hardware).
4. Build and run the application onto your device.
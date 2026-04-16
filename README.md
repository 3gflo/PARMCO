<center> <h1>Phone App RP4 Motor Control (PARMCO) 🚁⚙️</h1> </center>
<p align="center">
  <img src="https://github.com/user-attachments/assets/626228cd-6c54-4045-8396-aba54e180763" alt="animated" />
</p>

**PARMCO** is a full-stack embedded engineering project bridging mobile application development with low-level C hardware control. A Raspberry Pi 4 runs a C/Assembly backend that drives a 12V DC motor via an L293D H-Bridge, while a Kotlin Android app connects over Bluetooth RFCOMM to provide wireless control and live RPM telemetry.

The system supports both **open-loop manual control** and **closed-loop automatic control** using IR sensor feedback for precise RPM calculation and dynamic target-matching.

---

## 🚀 System Overview

The system architecture consists of a high-performance C server running on the Raspberry Pi and a Kotlin-based Android application. The two communicate via the **Bluetooth Serial Port Profile (SPP)**.

### Key Features
* **Low-Latency Control:** Non-blocking RFCOMM server for instantaneous motor response.
* **Precision PWM:** Hardware-level Pulse Width Modulation for smooth speed control (0–1000 range).
* **Live Telemetry:** High-speed IR sensor polling (10 kHz) to calculate and transmit real-time RPM.
* **Safety Integration:** Automatic hardware shutdown on Bluetooth disconnection.

---

## 📁 Project Structure

The project is organized into two primary functional layers:

| Component | Primary Files | Responsibility |
| :--- | :--- | :--- |
| **Firmware (C)** | bt_motor_control.c | GPIO management, hardware PWM, IR polling, and BT server logic. |
| **Mobile App (Kotlin)** | MainActivity.kt, activity_main.xml | Bluetooth client lifecycle, UI event handling, and telemetry visualization. |
| **Documentation** | MOTOR_README.md, ANDROID_README.md | Specific setup instructions for each subsystem. |
| **Hardware** | image_a9dc87.jpg | Schematic for L298N/L293D H-Bridge and IR sensor wiring. |

---

## 🔌 Hardware Configuration

The system relies on a Raspberry Pi connected to an H-Bridge motor driver and an optical encoder. For detailed wiring, refer to the **Hardware Schematic (image_a9dc87.jpg)**.

### BCM GPIO Mapping
* **Pin 18:** PWM Speed Control (Alt5)
* **Pin 23 & 24:** H-Bridge Directional Logic
* **Pin 25:** IR Encoder Input

---

## 📡 Communication Protocol

Data is exchanged as ASCII strings terminated by a newline (\n).

### App to Pi (Commands)
* STATE:START / STATE:STOP - Power toggle.
* DIR:FORWARD / DIR:REVERSE - H-Bridge polarity switch.
* RPM:[0-1000] - Sets the PWM duty cycle.

### Pi to App (Telemetry)
* MEASURED_RPM:[value] - Real-time speed updates sent every second.

---

## 🛠️ Quick Start

1.  **Prepare the Pi:** Install the `bcm2835` and `libbluetooth-dev` libraries.
2.  **Compile & Run:**
    gcc bt_motor_control.c -o bt_motor -lbcm2835 -lbluetooth
    sudo ./bt_motor
3.  **Mobile Setup:** Build the Android project in Android Studio and deploy to a physical device.
4.  **Connect:** Open the app, click "Connect to Pi", and select your Raspberry Pi from the discovered devices list.

---

> [!IMPORTANT]
> Always ensure the Raspberry Pi server is running with sudo privileges to allow direct access to the hardware registers and Bluetooth stack.

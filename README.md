# Phone App RP4 Motor Control (PARMCO) 🚁⚙️

[![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi)](https://www.raspberrypi.org/)
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)
[![Control Systems](https://img.shields.io/badge/Control-PID-brightgreen?style=for-the-badge)]()
[![Status](https://img.shields.io/badge/Status-Active-success?style=for-the-badge)]()

**AeroSync** is a full-stack embedded engineering project that bridges mobile application development with real-world hardware control. Utilizing a Raspberry Pi 4 (RP4) and a custom-designed MOSFET driver circuit, this system allows users to wirelessly control and monitor a DC motor and propeller setup via a dedicated Bluetooth mobile application. 

This project features both open-loop manual controls and closed-loop automatic controls using Infrared (IR) sensor feedback for precise RPM maintenance and dynamic target-matching.

---

## 📋 Table of Contents
1. [Key Features](#-key-features)
2. [System Architecture](#-system-architecture)
3. [Modes of Operation](#-modes-of-operation)
4. [Hardware Design & Circuit Analysis](#-hardware-design--circuit-analysis)
5. [Software Setup & Deployment](#-software-setup--deployment)
6. [Future Improvements](#-future-improvements)

---

## ✨ Key Features

### 📱 Mobile App Interface
A highly intuitive, user-friendly interface designed for comprehensive motor control:
* **Directional Control:** Toggle between Clockwise (CW) and Counter-Clockwise (CCW) rotation.
* **Granular Speed Control:** Faster/Slower manual overrides.
* **Power State:** Immediate Stop/Start functionality.
* **Target RPM Input:** Direct numerical input for desired motor speed.
* **Live Telemetry:** Real-time display of the *Actual RPM* streamed back from the RP4.
* **Mode Selector:** Seamless switching between Manual, Auto-Maintain, and Auto-Match modes.

### 📡 Robust Bluetooth (BLE) Communication
* **Persistent Pairing:** The app and RP4 handle standard Bluetooth handshake protocols, remembering the connection across sessions.
* **Self-Healing Connection:** If a device is "forgotten," the system can re-establish and save a new connection seamlessly.
* **Headless RP4 Operation:** The RP4 is configured to boot directly into a "quiet" state (motor off) upon powering up, independently running the server script and waiting for the app's BLE connection without requiring external monitors or keyboards.

---

## ⚙️ Modes of Operation

AeroSync operates in three distinct modes, showcasing different levels of control theory:

1. **Manual Mode (Open-Loop):** The user has direct control over the motor's PWM duty cycle using the "Speed" controls on the app. No active feedback is used to regulate speed against load changes.
2. **Auto/Maintain (Closed-Loop PID):** The user inputs a *Desired RPM*. The RP4 reads the *Actual RPM* using an IR sensor pointed at the motor's propeller. Using a PID (Proportional-Integral-Derivative) control algorithm, the RP4 dynamically adjusts the power to maintain the exact desired speed, regardless of minor aerodynamic load changes.
3. **Auto/Match (Dynamic Closed-Loop):** The system relies on a *second* IR sensor observing an external, independent propeller. The RP4 calculates the external propeller's RPM in real-time and continuously updates the PID controller's setpoint. Our motor autonomously adjusts its speed to perfectly mimic the external source.

---

## 🔌 Hardware Design & Circuit Analysis

### Components Used
* **Raspberry Pi 4 Model B** (Logic & Control)
* **DC Motor with Propeller**
* **N-Channel Logic-Level MOSFET** (e.g., IRLZ44N)
* **Flyback Diode** (e.g., 1N4007)
* **2x IR Obstacle Avoidance Sensors** (For RPM encoding)
* **Resistors** (Gate pull-down and current limiting)
* **External Power Supply** (For the motor)

### Circuit Setup & "How It Works" Analysis
To safely control a high-current DC motor using the low-current 3.3V GPIO pins of the Raspberry Pi 4, we utilize a **MOSFET as an electronic switch**. 

**1. The MOSFET Driver Circuit:**
* **The Gate:** A PWM (Pulse Width Modulation) signal from the RP4's GPIO is sent to the MOSFET's Gate. A resistor (e.g., 10kΩ) is placed between the Gate and Ground to act as a pull-down, ensuring the MOSFET turns completely off when the RP4 signal goes low (preventing floating states).
* **The Source:** Connected directly to the common Ground shared by the RP4 and the external motor power supply.
* **The Drain:** Connected to the negative terminal of the DC Motor. The positive terminal of the motor connects directly to the external power supply.
* **How it works:** When the RP4 sends a HIGH signal, the MOSFET "opens the gate," allowing current to flow from the external supply, through the motor, into the Drain, and out the Source to Ground, spinning the motor. By rapidly pulsing this signal (PWM), we control the average voltage, and thus the speed.

**2. Inductive Load Protection (Flyback Diode):**
* Motors are highly inductive. When the MOSFET switches off, the magnetic field in the motor collapses, creating a massive reverse voltage spike. 
* **How it works:** A Flyback Diode is placed in parallel with the motor (Cathode to positive, Anode to negative). This provides a safe path for the inductive kickback current to loop back and dissipate harmlessly, protecting the MOSFET and the fragile RP4 GPIO pins from destruction.

**3. IR Sensor Feedback:**
* The IR sensors emit an infrared beam that bounces off the propeller blades as they pass. The receiver detects this reflection, pulling a digital signal pin HIGH/LOW.
* **How it works:** The RP4 utilizes hardware interrupts to count these pulses over a set time interval, mathematically converting the pulse frequency into an accurate RPM reading.

---

## 🚀 Software Setup & Deployment

*(Note: Replace with your specific repository instructions)*

### Prerequisites
* Raspberry Pi OS (Bullseye or later)
* Python 3.9+
* Required Libraries: `RPi.GPIO`, `bluez`, `pybluez` (or specific BLE library used)

### Raspberry Pi Configuration (Headless Boot)
To ensure the RP4 boots into the application automatically:
1. Clone the repository to the RP4:
   ```bash
   git clone [https://github.com/YourUsername/AeroSync.git](https://github.com/YourUsername/AeroSync.git)

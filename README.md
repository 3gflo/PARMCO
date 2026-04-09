# Phone APP RP4 Motor Control (PARMCO) 🚁⚙️

[![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi)](https://www.raspberrypi.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)
[![Status](https://img.shields.io/badge/Status-Active-success?style=for-the-badge)]()

**PARMCO** is a full-stack embedded engineering project bridging mobile application development with low-level C hardware control. Utilizing a Raspberry Pi 4 (RP4), an L293D H-Bridge motor driver, and the `bcm2835` library, this system allows users to wirelessly control and monitor a 12V DC motor and propeller setup via a dedicated Bluetooth (RFCOMM) mobile application.

This project features both open-loop manual controls and closed-loop automatic controls using Infrared (IR) sensor feedback for precise RPM calculations and dynamic target-matching.

---

## 📋 Table of Contents
1. [Key Features](#-key-features)
2. [Hardware Architecture & Circuit](#-hardware-architecture--circuit)
3. [Software & Subsystems](#-software--subsystems)
4. [Bluetooth Protocol Definition](#-bluetooth-protocol-definition)
5. [Compilation & Deployment](#-compilation--deployment)

---

## ✨ Key Features

### 📱 Mobile App Interface
A highly intuitive interface designed for comprehensive motor control:
* **Directional Control:** Toggle between Clockwise and Counter-Clockwise rotation.
* **Granular Speed Control:** Adjust manual throttle parameters.
* **Target RPM Input:** Direct numerical input for desired motor speed.
* **Live Telemetry:** Real-time display of the *Actual RPM* streamed back from the RP4.
* **Mode Selector:** Seamless switching between Manual, Auto-Maintain, and Auto-Match (Synced) modes.

### 📡 Robust Bluetooth RFCOMM Communication
* **C-Based BlueZ Server:** The RP4 runs a lightweight C server utilizing `sys/socket.h` and `bluetooth/rfcomm.h` to accept incoming mobile connections.
* **Headless Telemetry:** The server processes asynchronous commands while maintaining a stable loop to stream `MEASURED_RPM` back to the app at 4Hz (250ms intervals).

---

## 🔌 Hardware Architecture & Circuit

The physical system isolates the low-voltage logic of the Raspberry Pi from the high-current demands of the motor using a dedicated driver IC. 

### Core Components
* **Raspberry Pi 4 Model B** (Logic & Control)
* **12V DC Motor with Propeller**
* **L293D Motor Driver IC** (H-Bridge)
* **IR Obstacle Avoidance Sensor** (For RPM encoding)
* **IRFZ34N N-Channel MOSFET** (For LED state indication)
* **Isolated Power Supplies:** 5V DC for logic, 12V DC for the motor.

### "How It Works" Circuit Analysis
1. **The L293D H-Bridge:** Instead of a single MOSFET, we use an H-Bridge to allow bi-directional motor control. 
   * **PWM (Speed):** The RP4 generates a hardware PWM signal (via `bcm2835`) sent to the `EN1` (Enable) pin of the L293D. Modulating this signal controls the overall voltage delivered to the motor.
   * **Direction:** Two standard GPIO pins connect to `IN1` and `IN2`. Setting `IN1` HIGH and `IN2` LOW drives the motor forward. Reversing these states reverses the motor.
2. **IR Sensor Feedback:** The IR sensor emits an infrared beam that bounces off the propeller blades. The receiver triggers a hardware interrupt (falling edge detection) on the RP4's GPIO 25. The code counts these pulses over a 1-second interval to calculate true RPM ($RPM = (\text{Pulses} \times 60) / 3$).
3. **MOSFET Indicator:** An IRFZ34N MOSFET is tied to the `OUT2` pin of the L293D. When that side of the H-bridge goes high, it triggers the MOSFET gate, sinking current for a 470Ω LED circuit. This acts as a visual hardware indicator of the motor's directional state.

*(Note: The schematic maps PWM to GPIO 22, IN1 to 17, and IN2 to 27. Ensure `basic_motor_control.c` macro definitions reflect these pins for final integration).*

---

## ⚙️ Software & Subsystems

The backend is written entirely in C for maximum performance and minimum latency. It is divided into two primary scripts:

### 1. `basic_motor_control.c` (Hardware Abstraction & Testing)
This script utilizes the `bcm2835` C library for direct memory access to the GPIO registers, bypassing Linux kernel overhead for precise microsecond timing.
* Handles hardware PWM generation.
* Manages non-blocking keyboard inputs (`kbhit`) for local terminal testing.
* Polls the IR sensor state at ~1000 Hz for highly accurate pulse-counting and RPM calculation.

### 2. `motor_server.c` (Communications)
This script binds a Bluetooth RFCOMM socket to Channel 1.
* Listens for incoming connections from the Android app.
* Parses delimited string commands (e.g., `DIR:FORWARD\n`).
* Simulates/calculates target RPM matching and writes telemetry data back to the client socket.

---

## 🗣️ Bluetooth Protocol Definition

The application and the Raspberry Pi communicate using simple, newline-terminated ASCII strings.

### Client (App) to Server (RP4) Commands
| Command Format | Description |
| :--- | :--- |
| `STATE:START` | Enables the motor / control loop. |
| `STATE:STOP` | Immediately cuts power to the motor. |
| `DIR:FORWARD` | Sets L293D IN1 HIGH, IN2 LOW. |
| `DIR:REVERSE` | Sets L293D IN1 LOW, IN2 HIGH. |
| `MODE:MANUAL` | Disables PID loop; direct PWM control. |
| `MODE:MAINTAIN`| Enables closed-loop PID to hold target RPM. |
| `MODE:SYNCED` | Matches RPM to a secondary external IR sensor. |
| `RPM:<value>` | Sets the target integer RPM (e.g., `RPM:1500`). |

### Server (RP4) to Client (App) Telemetry
| Message Format | Description |
| :--- | :--- |
| `MEASURED_RPM:<value>` | Streams the current live RPM back to the app UI. |

---

## 🚀 Compilation & Deployment

### Prerequisites
You must have the `bcm2835` library and `BlueZ` development headers installed on your Raspberry Pi:
```bash
sudo apt-get update
sudo apt-get install libbluetooth-dev

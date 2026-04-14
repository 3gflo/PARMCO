# Phone APP RP4 Motor Control (PARMCO) 🚁⚙️
![Generated Video](https://github.com/user-attachments/assets/626228cd-6c54-4045-8396-aba54e180763)

[![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi)](https://www.raspberrypi.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Kotlin](https://img.shields.io/badge/Kotlin-7F52FF?style=for-the-badge&logo=kotlin&logoColor=white)](https://kotlinlang.org/)
[![Bluetooth](https://img.shields.io/badge/-Bluetooth-0082FC?style=for-the-badge&logo=bluetooth)](https://www.bluetooth.com/)
[![Status](https://img.shields.io/badge/Status-Active-success?style=for-the-badge)]()

**PARMCO** is a full-stack embedded engineering project bridging mobile application development with low-level C hardware control. A Raspberry Pi 4 runs a C/Assembly backend that drives a 12V DC motor via an L293D H-Bridge, while a Kotlin Android app connects over Bluetooth RFCOMM to provide wireless control and live RPM telemetry.

The system supports both **open-loop manual control** and **closed-loop automatic control** using IR sensor feedback for precise RPM calculation and dynamic target-matching.

---

## 📋 Table of Contents
1. [Repository Structure](#-repository-structure)
2. [Hardware Architecture & Circuit](#-hardware-architecture--circuit)
3. [System Overview](#-system-overview)
4. [Quick Start](#-quick-start)

---

## 📁 Repository Structure

```
PARMCO/
├── README.md                   ← You are here
├── motor-control/              ← Raspberry Pi C/Assembly backend
│   └── README.md
└── android-app/                ← Android Kotlin application
    └── README.md
```

* **[`motor-control/`](./motor-control/README.md)** — All Raspberry Pi software: PWM generation, IR sensor polling, ARM Assembly PID controller, and GPIO pin mappings.
* **[`android-app/`](./android-app/README.md)** — The Android application, Bluetooth architecture, full communication protocol, and setup instructions.

---

## 🔌 Hardware Architecture & Circuit

The physical system isolates the low-voltage logic of the Raspberry Pi from the high-current demands of the motor using a dedicated driver IC.

### Core Components

| Component | Role |
| :--- | :--- |
| **Raspberry Pi 4 Model B** | Logic, control loop, and Bluetooth server |
| **12V DC Motor with Propeller** | Actuator (3-blade, used for RPM encoding) |
| **L293D Motor Driver IC** | H-Bridge for bi-directional motor control |
| **IR Obstacle Avoidance Sensor** | RPM encoder via propeller blade pulse counting |
| **IRFZ34N N-Channel MOSFET** | Visual directional state indicator (LED driver) |
| **5V DC supply** | Logic power for the Raspberry Pi |
| **12V DC supply** | Motor power, isolated from logic rail |

### Circuit Analysis

**Speed Control (PWM → L293D EN1):** The RP4 generates a hardware PWM signal sent to the `EN1` (Enable) pin of the L293D. Modulating the duty cycle controls the average voltage delivered to the motor, and therefore its speed.

**Direction Control (GPIO → L293D IN1/IN2):** Two GPIO pins connect to `IN1` and `IN2`. Setting `IN1` HIGH / `IN2` LOW drives the motor forward; reversing the states reverses the motor.

**RPM Feedback (IR Sensor → GPIO 25):** The IR sensor's beam reflects off the propeller blades, triggering a falling-edge interrupt on GPIO 25. Pulses are counted over a 1-second window and converted to RPM:

$$RPM = (\text{Pulse Count} \times 60) / 3$$

**Direction Indicator (L293D OUT2 → MOSFET gate):** When the H-Bridge's `OUT2` rail goes high, it triggers the IRFZ34N gate, sinking current through a 470Ω LED circuit as a hardware-level visual indicator of motor direction.

> **Pin Mapping Note:** There is an intentional discrepancy between the schematic and the final software. See [`motor-control/README.md`](./motor-control/README.md) for the definitive BCM GPIO pin assignments used in the code.

---

## 🧩 System Overview

```
  [Android App]  ──── Bluetooth RFCOMM ────  [Raspberry Pi 4]
       │                                            │
  Kotlin UI                                    C/ASM Backend
  - Mode select                                - BCM2835 PWM
  - Target RPM input                           - GPIO H-Bridge control
  - Live RPM display  ◄── MEASURED_RPM ──────  - IR pulse counting
                       ──── Commands ─────────  - PID (ARM Assembly)
```

The Android app sends ASCII commands (e.g., `RPM:1500`, `MODE:MAINTAIN`) over a persistent RFCOMM socket. The Pi parses these, drives the motor accordingly, and streams back `MEASURED_RPM:<value>` at 4Hz. See [`android-app/README.md`](./android-app/README.md) for the full protocol definition.

---

## 🚀 Quick Start

### Raspberry Pi — Install Dependencies
```bash
sudo apt-get update
sudo apt-get install libbluetooth-dev
# bcm2835 library must be built from source: http://www.airspayce.com/mikem/bcm2835/
```

### Build & Run
```bash
cd motor-control/
make
sudo ./motor_server
```

For detailed compilation options, Makefile targets, and standalone hardware testing, see [`motor-control/README.md`](./motor-control/README.md).

### Android App
Open the `android-app/` directory in Android Studio, build, and install on a Bluetooth-capable Android device. See [`android-app/README.md`](./android-app/README.md) for permission requirements and pairing instructions.

# 🍓 PARMCO: Motor Control

[![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi)](https://www.raspberrypi.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))

This directory contains the Raspberry Pi Bluetooth server for PARMCO. The software provides real-time motor speed control and telemetry using hardware PWM and high-speed IR sensor polling via the `bcm2835` library.

---

## 📂 Folder Contents

| File | Description |
| :--- | :--- |
| **`bt_motor_control.c`** | The primary application. Implements an RFCOMM Bluetooth server, hardware PWM generation, and non-blocking command processing for real-time motor control. |
| **`Makefile`** | Build script to compile the project and link required Bluetooth and GPIO libraries. |
| **`image_a9dc87.jpg`** | Hardware schematic showing the L293D H-Bridge and IR sensor wiring. |

---

## ⚙️ GPIO Pin Mappings

The software interfaces with hardware through the `bcm2835` library using memory-mapped GPIO access for precise microsecond timing. 

> **Important:** Wire the physical hardware to the **BCM GPIO** column below to match the software definitions.

| Component Function | Macro Name | BCM GPIO |
| :--- | :--- | :---: |
| PWM Speed Control → L293D EN1 | `PWM_PIN` | **18** |
| H-Bridge Forward → L293D IN1 | `DIR1_PIN` | **23** |
| H-Bridge Reverse → L293D IN2 | `DIR2_PIN` | **24** |
| IR Sensor Pulse Input | `IR_PIN` | **25** |

---

## 📡 Control & Telemetry Logic

### RPM Measurement
The system uses an IR Slotted Optical Encoder to measure speed. The software polls **GPIO 25** at approximately 10 kHz.
* **Pulse Logic**: One rotation is calculated based on 3 pulses (representing 3 blades/slots).
* **Calculation**: $RPM = (\\text{Pulse Count} \\times 60) / 3$.
* **Window**: Telemetry is averaged and transmitted back to the Android app every 1 second.

### Bluetooth Protocol
The server listens on **RFCOMM Channel 1**. It parses incoming string commands from the Android app:
* `STATE:START` / `STATE:STOP`: Toggle motor power.
* `DIR:FORWARD` / `DIR:REVERSE`: Switch H-Bridge logic.
* `RPM:[0-1000]`: Sets the PWM duty cycle (0% to 100%).
* **Outgoing**: The server sends `MEASURED_RPM:XXXX\\n` to the client every second.

---

## 🛠️ Compilation & Usage

### Prerequisites

1.  **BlueZ Dev Headers**:
    ```bash
    sudo apt-get update
    sudo apt-get install libbluetooth-dev
    ```

2.  **bcm2835 Library**:
    Install from [Airspayce](http://www.airspayce.com/mikem/bcm2835/) to allow direct hardware register access.

### Build and Run
Use the included Makefile for standard deployment:
```bash
# Compile the bt_motor_control executable
make

# Run with root privileges (required for GPIO and BT stack)
make run

# 🍓 PARMCO: Motor Control Software & Backend

[![Raspberry Pi](https://img.shields.io/badge/-Raspberry_Pi-C51A4A?style=for-the-badge&logo=Raspberry-Pi)](https://www.raspberrypi.org/)
[![C](https://img.shields.io/badge/C-00599C?style=for-the-badge&logo=c)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Assembly](https://img.shields.io/badge/Assembly-000000?style=for-the-badge&logo=assembly)](https://en.wikipedia.org/wiki/Assembly_language)

This directory contains the low-level embedded software for the **Phone APP RP4 Motor Control (PARMCO)** project. The codebase runs on a Raspberry Pi 4 (RP4) and is responsible for bridging wireless Bluetooth commands with physical hardware execution using the `bcm2835` C library and custom ARM assembly routines.

---

## 📂 Folder Contents

* **`basic_motor_control.c`**: A standalone C program designed to test hardware limits. It utilizes memory-mapped GPIO access to generate hardware PWM, read keyboard inputs non-blockingly, and poll an IR sensor at ~1000 Hz for real-time RPM calculation.
* **`motor_server.c`**: The Bluetooth SPP (Serial Port Profile) server utilizing the `BlueZ` stack. It binds an RFCOMM socket to Channel 1, parses delimited string commands from the Android application, and manages the main control loop/telemetry stream.
* **`control.S`**: An ARM Assembly file containing the highly optimized `adjust_pwm` function. It acts as a proportional controller, calculating the error between the target and current RPM and adjusting the PWM signal with dynamic gain based on the speed tier.
* **`Makefile`**: Build automation script to compile the C and Assembly sources into a single executable and run it with the necessary root privileges.
* **`image_a9dc87.jpg`**: The official hardware schematic detailing the L293D H-Bridge, IRFZ34N MOSFET indicator, and power isolation.

---

## ⚙️ Hardware Interface & Pinout

The software interfaces with the physical world primarily through the `bcm2835` library. 

**Important Note on Pin Mappings:** There is an intentional discrepancy between the testing schematic and the final software definitions. If wiring the physical hardware, please adhere to the software definitions below to ensure the code executes correctly:

| Component Function | Variable Name | BCM GPIO Pin (Software) | Schematic Pin |
| :--- | :--- | :--- | :--- |
| **PWM Speed Control** | `PWM_PIN` | **18** | 22 |
| **H-Bridge IN1 (Forward)** | `DIR1_PIN` | **23** | 17 |
| **H-Bridge IN2 (Reverse)**| `DIR2_PIN` | **24** | 27 |
| **IR Sensor Pulse In** | `IR_PIN` | **25** | 25 |

### RPM Encoding Logic
The system calculates real-time RPM using a standard 3-pin IR Obstacle Avoidance Sensor. The `basic_motor_control.c` script polls GPIO 25. When the signal drops from `HIGH` to `LOW` (falling edge), a pulse is counted. Because the propeller has 3 blades, the software calculates RPM every 1 second using: 
$RPM = (\text{Pulse Count} \times 60) / 3$

---

## 🧠 Assembly Control Logic (`control.S`)

To ensure maximum performance in the closed-loop state, the proportional error calculation is offloaded to a custom ARM Assembly routine. 

The C code calls `int adjust_pwm(int current_rpm, int target_rpm, int current_pwm, int pwm_range);`, which maps to registers `r0` through `r3`. The assembly routine implements dynamic gain:
1. **Error Calculation:** Subtracts current RPM from target RPM.
2. **Dynamic Gain:**
   * **Gentle Gain (Target < 1500 RPM):** Divides the error by 32 (via Arithmetic Shift Right `asr #5`) to prevent aggressive oscillation at low speeds.
   * **Aggressive Gain (Target >= 1500 RPM):** Divides the error by 16 (`asr #4`) to provide enough torque to reach high speeds quickly.
3. **Clamping:** Safely clamps the resulting PWM value between 0 and the defined `pwm_range` (1024) before returning to the C loop.

---

## 🛠️ Compilation & Usage

### Prerequisites
Ensure the `bcm2835` and `BlueZ` libraries are installed on your Raspberry Pi:
```bash
sudo apt-get update
sudo apt-get install libbluetooth-dev
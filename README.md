# NAVISECURE: Code Deployment & Operational Guide

This repository contains the multi-device source code and integration configuration for **Navisecure** — an IoT-Based Truck Driver Safety & Monitoring System.

The project uses a distributed processing topology across:

* Raspberry Pi
* ESP32
* Arduino Mega 2560

This architecture ensures:

* Computer vision tasks run independently
* Wireless proximity tracking operates continuously
* Safety-critical sensor processing remains real-time
* No major processing bottlenecks occur during operation

---

# 1. System Topology & Communication Flow

## Overall System Flow Diagram

```text
NAVISECURE VEHICLE START & MONITORING PIPELINE
Distributed Multi-Node Verification & Fail-Safe Architecture
```

---

## Node Architecture Overview

### Raspberry Pi

Responsible for running resource-intensive:

* Local Binary Patterns (LBP)
* OpenCV face verification
* `face_recognition` processing

It sends identity signals over USB-Serial:

```text
AUTH_SUCCESS
AUTH_FAILED
```

---

### ESP32 DevKit

Dedicated exclusively to:

* Bluetooth Classic (SPP) communication
* Driver proximity verification
* Smartphone authentication

It continuously checks for the driver's registered mobile device and sends status updates to the Arduino Mega through UART.

---

### Arduino Mega 2560

Acts as the **central system controller**.

Responsibilities include:

* State machine execution
* Safety diagnostics
* Ignition relay control
* GPS parsing
* Emergency alert handling
* GSM communication
* Crash monitoring

---

# 2. Hardware Wiring Configuration

## Pin Mapping Table

| From Component | Component Pin   | To Component       | Component Pin | Connection Notes          |
| -------------- | --------------- | ------------------ | ------------- | ------------------------- |
| Arduino Mega   | 5V              | Bus Power          | 5V Rail       | System logic power        |
| Arduino Mega   | GND             | Bus Power          | GND Rail      | Common ground             |
| Arduino Mega   | Pin 8           | Ignition Relay     | IN / Trigger  | Controls ignition circuit |
| Arduino Mega   | Pin 18 (TX1)    | SIM800L GSM        | RXD           | Requires level shifting   |
| Arduino Mega   | Pin 19 (RX1)    | SIM800L GSM        | TXD           | Standard serial lines     |
| Arduino Mega   | Pin 17 (RX2)    | ESP32              | GPIO 17 (TX2) | UART communication        |
| Arduino Mega   | Pin 10 (SoftTX) | Neo-6M GPS         | RXD           | SoftwareSerial            |
| Arduino Mega   | Pin 11 (SoftRX) | Neo-6M GPS         | TXD           | Receives NMEA strings     |
| Arduino Mega   | Pin 20 (SDA)    | MPU6050 / LCD      | SDA           | I2C data bus              |
| Arduino Mega   | Pin 21 (SCL)    | MPU6050 / LCD      | SCL           | I2C clock bus             |
| Arduino Mega   | A0              | TPMS Potentiometer | Wiper         | Tire pressure simulation  |
| Arduino Mega   | A1              | Load Potentiometer | Wiper         | Load sensor simulation    |
| Arduino Mega   | A2              | Temp Sensor        | Signal        | Thermal monitoring        |

---

## Voltage Level Shifting Note

The ESP32 and SIM800L modules operate at:

```text
3.3V Logic
```

The Arduino Mega operates at:

```text
5V Logic
```

Although the Mega can safely read 3.3V signals, the reverse is unsafe.

Use a resistor divider network:

```text
10kΩ / 20kΩ
```

on Arduino TX lines connected to:

* ESP32 RX
* SIM800L RX

to avoid permanent hardware damage.

---

# 3. Deployment Instructions

# Phase A — Raspberry Pi Setup (Face Verification Node)

The Raspberry Pi acts as the edge AI processing unit.

---

## 1. Software Setup

Update the Raspberry Pi and install required dependencies:

```bash
sudo apt-get update
sudo apt-get install cmake -y

pip3 install opencv-python face_recognition pyserial
```

---

## 2. Driver Image Placement

Save a clear frontal image of the authorized driver.

The image must be named exactly:

```text
driver_template.jpg
```

Place it inside the same directory as:

```text
navisecure_face_auth.py
```

Example structure:

```text
/home/pi/navisecure/
│
├── navisecure_face_auth.py
└── driver_template.jpg
```

---

## 3. Execution

Start the authentication loop:

```bash
python3 navisecure_face_auth.py
```

The script remains idle until the Arduino controller sends:

```text
TRIGGER_AUTH
```

This reduces unnecessary CPU usage and camera wear.

---

# Phase B — ESP32 Setup (Bluetooth Verification)

The ESP32 manages smartphone proximity verification.

---

## 1. Library Installation

Inside Arduino IDE:

* Install ESP32 Board Manager
* Ensure `BluetoothSerial` library is available

No external downloads are required.

---

## 2. Configuration

Open:

```text
navisecure_esp32_bt.ino
```

Update the driver's Bluetooth device name:

```cpp
const String TARGET_PHONE_NAME = "NaviSecure_Driver_Device";
```

Ensure the smartphone remains discoverable.

---

## 3. Flash the ESP32

Upload the firmware.

The ESP32 will continuously monitor:

* Bluetooth pairing state
* Connection heartbeat
* Driver proximity

and transmit:

```text
BT_PAIRED
BT_LOST
BT_OK
```

to the Arduino Mega via UART.

---

# Phase C — Arduino Mega Setup (Master Controller)

The Mega coordinates all verification and fail-safe operations.

---

## 1. Required Libraries

Install the following libraries through Arduino IDE:

```text
LiquidCrystal_I2C
Wire
SoftwareSerial
```

---

## 2. Emergency Contact Configuration

Open:

```text
navisecure_arduino_controller.ino
```

Locate:

```cpp
sendRescueSMS()
dialEmergencyContact()
```

Replace the emergency number:

```cpp
Serial1.println("AT+CMGS=\"+919900990099\"");
```

with your actual emergency contact number.

---

## 3. Upload Firmware

Flash the firmware to:

```text
Arduino Mega 2560
```

The system will immediately begin initialization and authentication routines.

---

# 4. In-Depth Code Explanations

# 1. Face Verification Engine (`navisecure_face_auth.py`)

## Trigger-Based Activation

The Raspberry Pi remains idle until it receives:

```text
TRIGGER_AUTH
```

from the Arduino controller.

This prevents:

* CPU overheating
* Continuous camera operation
* Unnecessary inference cycles

---

## Biometric Processing

The script:

1. Captures frames using OpenCV
2. Resizes frames to 25%
3. Detects faces using:

   * HOG descriptors
   * Linear SVM classifiers
4. Generates 128-dimensional facial embeddings

---

## Authentication Logic

The generated face vector is compared against:

```text
driver_template.jpg
```

using Euclidean distance comparison.

If the threshold is satisfied:

```text
AUTH_SUCCESS
```

is transmitted back to the Arduino.

Otherwise:

```text
AUTH_FAILED
```

is returned.

---

# 2. Proximity Handler (`navisecure_esp32_bt.ino`)

## Bluetooth SPP Communication

The ESP32 creates a Classical Bluetooth SPP server using its onboard radio module.

---

## Connection Heartbeat

The firmware continuously checks:

* Pairing state
* Link integrity
* Connection loss

Status messages include:

```text
BT_OK
BT_PAIRED
BT_LOST
```

These are sent to the Arduino Mega via Serial2.

---

# 3. Master Controller FSM (`navisecure_arduino_controller.ino`)

The firmware uses an asynchronous finite state machine (FSM).

---

## STATE_WAITING_AUTH

Actions:

* LCD status display
* Trigger Raspberry Pi authentication
* Trigger Bluetooth verification

---

## STATE_PREDRIVE_DIAGNOSTICS

Reads sensor data from:

```text
A0 → Tire Pressure
A1 → Vehicle Load
A2 → Engine Temperature
```

Thresholds:

```text
85°C   → Engine Temperature Limit
800    → Cargo Load Limit
300    → Pressure Threshold
```

If abnormal values are detected:

```text
Ignition Relay = LOCKED
```

---

## STATE_READY_TO_START

The ignition relay is enabled:

```cpp
digitalWrite(8, HIGH);
```

allowing vehicle startup.

---

## STATE_TRANSIT_ACTIVE

The MPU6050 computes resultant G-force using:

```math
\text{Vector G-Force} = \sqrt{x^2 + y^2 + z^2}
```

If acceleration exceeds:

```text
4.5G
```

the system assumes a major collision and switches state.

---

## STATE_CRASH_ALERT

The system begins a:

```text
60-second recovery countdown
```

If the driver does not cancel the alert:

* GPS coordinates are fetched
* Emergency SMS is transmitted
* Emergency contact is dialed automatically

using the SIM800L GSM module.

---

---

# Technologies Used

* Python
* OpenCV
* face_recognition
* Arduino C++
* Bluetooth Classic SPP
* GSM Communication
* GPS Tracking
* Embedded Systems
* IoT Monitoring
* UART Communication
* I2C Sensors
* Finite State Machines

---

# Core Features

✅ Face Authentication
✅ Bluetooth Driver Verification
✅ Crash Detection
✅ GPS Emergency Tracking
✅ GSM Emergency Alerting
✅ Ignition Lock Control
✅ Real-Time Diagnostics
✅ Distributed Embedded Architecture
✅ Multi-Node Communication
✅ Safety Fail-Safe System

---

# License

This project is intended for:

* Academic Research
* Embedded Systems Learning
* IoT Demonstration
* Vehicle Safety Research

Modify and deploy responsibly.

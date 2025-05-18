# EMG-Based Wearable Control System

This project implements a sophisticated wearable control system that processes electromyography (EMG) signals to control external devices through wireless communication. The system comprises EMG sensors, a signal processing module, and a microcontroller that communicates with actuators.

## System Overview

The system utilizes ESP32 microcontrollers to process EMG sensor data and control actuators. Key features include:

- BLE connectivity for smartphone/tablet control
- ESP-NOW wireless communication between microcontroller units
- Real-time visual feedback via OLED display
- Haptic feedback through vibration motors
- Remote control of servo motors based on muscle activity

## System Architecture

```mermaid
graph TD
    A[EMG Sensors] -->|Raw Signals| B[Signal Processing Module]
    B -->|Processed Signals| C[ESP32 Microcontroller]
    C -->|BLE| D[Smartphone/Tablet]
    D -->|Commands| C
    C -->|ESP-NOW| E[Secondary ESP32]
    C -->|Control| F[Vibration Motor]
    C -->|Display| G[OLED Screen]
    E -->|Control| H[Servo Motors/Actuators]
    
    
    
    class A sensors;
    class B processing;
    class C,D,E controllers;
    class F,G,H output;
```

## Communication Flow

```mermaid
sequenceDiagram
    participant EMG as EMG Sensors
    participant ESP32 as Primary ESP32
    participant BLE as BLE Device
    participant SecESP as Secondary ESP32
    participant Motors as Servo Motors
    
    EMG->>ESP32: Send muscle activity signals
    ESP32->>ESP32: Process signals
    
    alt Manual Control via BLE
        BLE->>ESP32: Send control commands
        ESP32->>ESP32: Validate commands
    end
    
    ESP32->>ESP32: Generate motor control signals
    ESP32->>SecESP: Send motor control via ESP-NOW
    SecESP->>Motors: Control servo positions
    ESP32->>ESP32: Update OLED display
    ESP32->>ESP32: Trigger haptic feedback
```

## Data Flow Diagram

```mermaid
flowchart LR
    A[EMG Sensors] --> B{Signal Processing}
    B -->|Processed Data| C[ESP32 Controller]
    
    C -->|BLE| D[User Interface]
    D -->|Commands| C
    
    C -->|ESP-NOW| E[Motor Control ESP32]
    
    C --> F[OLED Display]
    C --> G[Vibration Motor]
    
    E --> H[Servo Motor 1]
    E --> I[Servo Motor 2]
    E --> J[Servo Motor 3]
    
   
```

## Hardware Components

- ESP32 microcontroller (Primary and Secondary)
- EMG sensors for muscle activity detection
- SSD1306 OLED display (128x64 pixels)
- Vibration motor for haptic feedback
- Servo motors for physical actuation
- Power supply circuit

## Software Features

- BLE server for wireless control
- ESP-NOW communication for low-latency device-to-device communication
- Command parsing and execution
- Real-time feedback through OLED display
- Motor control with position commands

## Usage

### BLE Commands

The system accepts the following commands via BLE:

- **Motor Control**: Format `M[1-3]:[0-180]` (e.g., `M1:90` sets motor 1 to 90 degrees)
- **Emergency Stop**: Send `STOP` to halt all motors

### ESP-NOW Messages

The system uses the following message types for ESP-NOW communication:

- `m`: Display message and optionally vibrate
- `v`: Trigger vibration only
- `o`: Override motor position
- `s`: Emergency stop all motors

## Setup and Configuration

1. Update the `receiverMacAddress` variable with your secondary ESP32's MAC address
2. Flash the code to your primary ESP32
3. Flash the corresponding receiver code (not included) to your secondary ESP32
4. Connect the hardware components according to the pin definitions in the code

## Pin Configuration

- OLED SDA: GPIO 23
- OLED SCL: GPIO 19
- Vibration Motor: GPIO 18

## Dependencies

- ESP-NOW library
- WiFi library
- Wire library
- Adafruit_GFX library
- Adafruit_SSD1306 library
- BLE device libraries

# ElderGuard - IoT Health Monitoring System for Elderly Care

![ElderGuard Logo](https://github.com/aland-omed/ElderGuardesp32devkitv1/raw/main/docs/images/logo.png)

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/Version-3.0-green.svg)](https://github.com/aland-omed/ElderGuardesp32devkitv1)
[![Platform](https://img.shields.io/badge/Platform-ESP32-red.svg)](https://www.espressif.com/en/products/socs/esp32)

## Overview

ElderGuard is a comprehensive IoT-based health monitoring system designed specifically for elderly care. Built on the ESP32 platform, the system provides real-time health monitoring, fall detection, location tracking, and medication reminders to help elderly individuals maintain independence while ensuring their safety and well-being.

## Key Features

- **Health Monitoring**
  - Real-time ECG and heart rate monitoring
  - Continuous vital sign tracking
  - Health data logging and analysis

- **Safety Features**
  - Advanced fall detection algorithm using MPU6050
  - Emergency alert system
  - Real-time GPS location tracking

- **Medication Management**
  - Scheduled medication reminders
  - Compliance tracking
  - Configurable dosage information

- **User Interface**
  - OLED display with intuitive information layout
  - Voice alerts and notifications
  - Simple control interface for elderly users

- **Connectivity**
  - Wi-Fi for home network connection
  - MQTT for real-time data transmission
  - HTTP for web-based monitoring interface
  - OTA firmware updates

## Hardware Components

| Component | Purpose | Interface |
|-----------|---------|-----------|
| ESP32 DevKit V1 | Main controller | - |
| AD8232 ECG Module | Heart monitoring | Analog |
| MPU6050 Accelerometer | Fall detection | I2C |
| NEO-6M GPS Module | Location tracking | Serial (UART) |
| SSD1306 OLED Display | User interface | I2C |
| MP3-TF-16P Module | Audio notifications | Serial (UART) |
| LiPo Battery | Power source | - |

## System Architecture

ElderGuard uses a multi-tasking architecture based on FreeRTOS:

```
                  ┌─────────────────┐
                  │     ESP32       │
                  │  Main Controller│
                  └────────┬────────┘
                           │
       ┌───────────────────┼───────────────────┐
       │                   │                   │
┌──────▼──────┐     ┌──────▼──────┐     ┌──────▼──────┐
│  Sensing    │     │  Processing  │     │ Communication│
│  Tasks      │     │  Tasks       │     │  Tasks       │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
┌──────▼──────┐     ┌──────▼──────┐     ┌──────▼──────┐
│ ECG Task    │     │Fall Detection│     │ WiFi Task   │
│ GPS Task    │     │Medication    │     │ MQTT Task   │
└─────────────┘     │Time Task     │     │ HTTP Task   │
                    └─────────────┘     └─────────────┘
```

### Task Organization

The system is organized into multiple FreeRTOS tasks, each handling a specific function:

- **Sensing Tasks**
  - `ecg_task`: Monitors heart signals and processes ECG data
  - `gps_task`: Handles GPS data acquisition and location tracking

- **Processing Tasks**
  - `fall_detection_task`: Processes accelerometer data to detect falls
  - `medication_task`: Manages medication schedules and reminds
  - `time_task`: Maintains system time via NTP

- **Interface Tasks**
  - `screen_task`: Manages the OLED display
  - `audio_task`: Controls the audio notification system

- **Connectivity Tasks**
  - `wifi_task`: Handles WiFi connectivity
  - `mqtt_task`: Manages MQTT communication
  - `http_task`: Provides HTTP server functionality
  - `firmware_update_task`: Handles OTA updates

## Setup and Installation

### Prerequisites
- PlatformIO IDE (recommended) or Arduino IDE
- ESP32 board support package
- Required libraries (see below)

### Required Libraries
All dependencies are managed through PlatformIO:
- Adafruit GFX Library
- Adafruit SH110X
- Adafruit MPU6050
- Adafruit Unified Sensor
- TinyGPSPlus
- DFRobotDFPlayerMini
- NTPClient
- PubSubClient
- ArduinoJson
- WebSockets

### Hardware Setup
1. Connect components according to the pin definitions in `include/config.h`
2. Ensure proper power distribution
3. Verify all sensor connections

### Software Installation
1. Clone this repository:
   ```
   git clone https://github.com/aland-omed/ElderGuardesp32devkitv1.git
   ```
2. Open the project in PlatformIO
3. Build and upload to your ESP32

## Configuration

The system is configured through the `config.h` file, which contains:
- Pin definitions for all connected hardware
- Network settings
- MQTT broker information
- System parameters and thresholds

## Usage Guide

1. Power on the device
2. Wait for system initialization (displayed on OLED)
3. The system will automatically:
   - Connect to WiFi
   - Synchronize time
   - Start all monitoring tasks
4. View system status on the OLED display
5. Alerts will be provided through:
   - Audio notifications
   - OLED display
   - MQTT messages to connected services

## Project Structure

```
ElderGuard/
├── include/                  # Header files
│   ├── audio_task.h          # Audio notifications
│   ├── config.h              # System configuration
│   ├── ecg_task.h            # ECG monitoring
│   ├── fall_detection_task.h # Fall detection algorithms
│   ├── firmware_update_task.h# OTA update functionality
│   ├── globals.h             # Global variables & structures
│   ├── gps_task.h            # GPS location tracking
│   ├── http_task.h           # HTTP server implementation
│   ├── medication_task.h     # Medication reminders
│   ├── mqtt_task.h           # MQTT client implementation
│   ├── screen_task.h         # OLED display controller
│   ├── time_task.h           # NTP time synchronization
│   └── wifi_task.h           # WiFi connectivity
├── src/                      # Source files
│   ├── globals.cpp           # Global variables implementation
│   ├── main.cpp              # Main program entry point
│   └── tasks/                # Task implementations
│       ├── audio_task.cpp    # Audio system implementation
│       ├── ecg_task.cpp      # ECG monitoring implementation
│       ├── fall_detection_task.cpp # Fall detection implementation
│       ├── firmware_update_task.cpp # OTA updates implementation
│       ├── gps_task.cpp      # GPS task implementation
│       ├── http_task.cpp     # HTTP server implementation
│       ├── medication_task.cpp # Medication reminder implementation
│       ├── mqtt_task.cpp     # MQTT client implementation
│       ├── screen_task.cpp   # OLED display implementation
│       ├── time_task.cpp     # Time synchronization implementation
│       └── wifi_task.cpp     # WiFi connection handling
├── platformio.ini            # PlatformIO configuration
├── COPYRIGHT.md              # Copyright and license information
└── README.md                 # Project documentation
```

## Development Status

- Current Version: V3.0 (April 2025)
- Status: Active Development
- Last Updated: April 26, 2025

## Future Enhancements

- Cloud integration for data storage and analysis
- Mobile app companion for remote monitoring
- Machine learning for improved fall detection
- Additional health sensors integration
- Battery optimization for extended runtime

## Authors

- **Aland Omed** - [GitHub](https://github.com/aland-omed)
- **Harun Hameed**
- **Zhir Jamil**

Project developed as a graduation project at Komar University of Science and Technology.

## License

This project is licensed under the MIT License - see the [COPYRIGHT.md](COPYRIGHT.md) file for details.

## Acknowledgments

- Komar University of Science and Technology for supporting this project
- All contributors to the open-source libraries used in this project
- The ESP32 community for their extensive resources and examples

---

*ElderGuard - Caring technology for independent living*
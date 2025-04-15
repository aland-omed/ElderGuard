# ElderGuard - IoT Health Monitoring System for Elderly Care

![ElderGuard Logo](https://github.com/aland-omed/ElderGuardesp32devkitv1/raw/main/docs/images/logo.png)

## Overview

ElderGuard is a comprehensive IoT-based health monitoring system designed specifically for elderly care. Built on the ESP32 platform, the system provides real-time health monitoring, fall detection, location tracking, and medication reminders to help elderly individuals maintain independence while ensuring their safety and well-being.

## Features

- **ECG Monitoring**: Real-time heart rate and ECG signal monitoring
- **Fall Detection**: Automatic detection of falls using accelerometer data
- **GPS Tracking**: Location monitoring for outdoor activities
- **Medication Reminders**: Scheduled alerts for medication
- **Audio Alerts**: Voice notifications for important events
- **OLED Display**: User interface for status information
- **Multi-tasking Architecture**: Built on FreeRTOS for reliable performance

## Hardware Components

- ESP32 DevKit V1 (Main controller)
- AD8232 ECG Module
- MPU6050 Accelerometer/Gyroscope
- NEO-6M GPS Module
- OLED Display (SSD1306)
- MP3-TF-16P Audio Module
- Various connecting wires and power management components

## System Architecture

ElderGuard uses a task-based architecture with FreeRTOS:

- **ECG Task**: Monitors heart rate and ECG signals
- **Fall Detection Task**: Processes accelerometer data to detect falls
- **GPS Task**: Handles location tracking
- **Medication Task**: Manages medication schedules and reminders
- **Screen Task**: Controls the OLED display interface
- **Audio Task**: Manages audio notifications and alerts

Tasks communicate using FreeRTOS queues and are synchronized with semaphores for shared resources.

## Setup and Installation

### Prerequisites
- PlatformIO IDE (recommended) or Arduino IDE
- ESP32 board support package
- Required libraries (listed in platformio.ini)

### Hardware Setup
1. Connect all components according to the pin definitions in `include/config.h`
2. Power the system using a suitable power source (LiPo battery recommended)

### Software Installation
1. Clone this repository:
   ```
   git clone https://github.com/aland-omed/ElderGuardesp32devkitv1.git
   ```
2. Open the project in PlatformIO or Arduino IDE
3. Install the required dependencies
4. Upload the firmware to your ESP32

## Usage

1. Power on the ElderGuard device
2. The system will automatically initialize and start all monitoring tasks
3. The OLED display shows current status information
4. Audio alerts will sound for important events (falls, medication times)

## Project Structure

```
ElderGuard/
├── include/            # Header files
│   ├── audio_task.h    # Audio notification system
│   ├── config.h        # System configuration and pin definitions
│   ├── ecg_task.h      # ECG monitoring functionality
│   ├── fall_detection_task.h # Fall detection algorithms
│   ├── gps_task.h      # GPS tracking functionality
│   ├── medication_task.h # Medication reminder system
│   └── screen_task.h   # OLED display interface
├── src/                # Source files
│   ├── main.cpp        # Main application entry point
│   └── tasks/          # Implementation of system tasks
│       ├── audio_task.cpp
│       ├── ecg_task.cpp
│       ├── fall_detection_task.cpp
│       ├── gps_task.cpp
│       ├── medication_task.cpp
│       └── screen_task.cpp
├── platformio.ini      # PlatformIO configuration
├── COPYRIGHT.md        # Copyright and license information
└── README.md           # Project documentation
```

## Contributing

This project was developed as a graduation project at Komar University of Science and Technology. While it is primarily for educational purposes, we welcome suggestions and improvements through GitHub issues and pull requests.

## Authors

- **Aland Omed** - [GitHub](https://github.com/aland-omed)
- **Harun Hameed**
- **Zhir Jamil**

## License

Copyright © 2025 Komar University of Science and Technology. All Rights Reserved.

This project is licensed for educational and non-commercial use only. See the [COPYRIGHT.md](COPYRIGHT.md) file for details.

## Acknowledgments

- Komar University of Science and Technology
- All open-source libraries used (Arduino Core for ESP32, Adafruit libraries, TinyGPS++, ArduinoJson, PubSubClient)

---

*ElderGuard - Caring technology for independent living*
/**
 * ElderGuard - Configuration File
 * 
 * This file contains all pin definitions, constants, and configuration parameters
 * for the ElderGuard system.
 */

#ifndef CONFIG_H
#define CONFIG_H

// Pin Definitions
// ------------------------------

// ECG Module (AD8232)
#define ECG_PIN 36          // Analog input
#define ECG_LO_POS_PIN 32   // Lead-off detection positive
#define ECG_LO_NEG_PIN 33   // Lead-off detection negative

// MPU6050 Accelerometer (I2C)
#define MPU_SDA_PIN 21
#define MPU_SCL_PIN 22
#define MPU_INT_PIN 39      // Interrupt pin for fall detection

// OLED Display (I2C, shares bus with MPU6050)
#define OLED_SDA_PIN 21
#define OLED_SCL_PIN 22
#define OLED_RESET_PIN -1
#define OLED_ADDRESS 0x3C

// GPS Module (GY-NEO6MV2)
#define GPS_RX_PIN 16       // ESP32 RX pin connected to GPS TX
#define GPS_TX_PIN 17       // ESP32 TX pin connected to GPS RX

// MP3 Player Module (MP3-TF-16P)
#define MP3_RX_PIN 4        // ESP32 RX pin connected to MP3 TX
#define MP3_TX_PIN 2        // ESP32 TX pin connected to MP3 RX

// System Constants
// ------------------------------

// Task Frequencies
#define ECG_SAMPLE_FREQUENCY_HZ 50     // 50Hz sampling rate for ECG
#define GPS_UPDATE_INTERVAL_MS 1000     // GPS update every 1 second
#define MQTT_PUBLISH_INTERVAL_MS 1000   // MQTT publishing every 1 second
#define HTTP_PUBLISH_INTERVAL_MS 40000  // HTTP publishing every 40 seconds
#define FALL_DETECTION_SAMPLE_RATE_HZ 50 // 50Hz sampling for accelerometer

// Audio File Mappings (on SD card)
#define AUDIO_WELCOME 1
#define AUDIO_MEDICATION 2
#define AUDIO_FALL_DETECTED 3
#define AUDIO_EMERGENCY 4

// Data Structures
// ------------------------------

// ECG Data Structure
typedef struct {
    int rawValue;
    int heartRate;
    bool validSignal;
    unsigned long timestamp;
} EcgData;

// GPS Data Structure
typedef struct {
    float latitude;
    float longitude;
    float altitude;
    float speed;
    int satellites;
    bool validFix;
    unsigned long timestamp;
} GpsData;

// Fall Detection Data
typedef struct {
    bool fallDetected;
    float acceleration;
    float orientation[3];  // pitch, roll, yaw
    unsigned long timestamp;
    int fallSeverity;      // 1-10 scale
} FallEvent;

// Medication Reminder
typedef struct {
    char name[32];         // Medication name
    unsigned long time;    // Scheduled time (epoch)
    bool taken;            // Whether medication was taken
} MedicationReminder;

// Audio Command
typedef struct {
    int fileNumber;
    int repeatCount;
    int volume;
} AudioCommand;

#endif // CONFIG_H
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
#define HTTP_PUBLISH_INTERVAL_MS 30000  // HTTP publishing every 30 seconds
#define FALL_DETECTION_SAMPLE_RATE_HZ 50 // 50Hz sampling for accelerometer

// Audio Settings
#define AUDIO_MAX_VOLUME 30             // Maximum volume level (0-30)

// Audio File Mappings (on SD card)
#define AUDIO_WELCOME 7
#define AUDIO_MEDICATION 6
#define AUDIO_FALL_DETECTED 2
#define AUDIO_EMERGENCY 1

// API Configuration
// ------------------------------
#define PATIENT_ID 1                          // Patient identifier for API communication
#define MAX_MEDICATIONS 20                    // Maximum number of medications to track
#define MEDICATION_FETCH_INTERVAL_MS 900000   // Fetch medication schedule every 15 minutes (900000ms)
#define MEDICATION_API_URL "https://elderguard.codecommerce.info/api/medications" // API endpoint for medication schedules

// Medication Task Constants
#define API_CHECK_INTERVAL 900000            // 15 minutes in milliseconds
#define TIME_CHECK_INTERVAL 10000            // 10 seconds in milliseconds
#define HTTP_TIMEOUT 10000                   // 10 seconds HTTP request timeout
#define MAX_RESPONSE_SIZE 8192               // Maximum API response size in bytes
#define MAX_JSON_DOC_SIZE 2048               // Size for JSON documents
#define MIN_SPIFFS_SPACE 4096                // Minimum required space in SPIFFS before cleaning
#define AUDIO_REPEAT_COUNT 5                 // Number of times to repeat medication audio alert

// WiFi and Time Management Constants
// ------------------------------
#define WIFI_SSID "Company 2.4"        // Default SSID
#define WIFI_PASSWORD "Halist2004"      // Default password
#define WIFI_CONNECT_TIMEOUT_MS 30000         // WiFi connection timeout (30 sec)
#define WIFI_RECONNECT_INTERVAL_MS 60000      // Attempt reconnection every minute
#define WIFI_TASK_INTERVAL_MS 5000            // Check WiFi status every 5 seconds

#define NTP_SERVER "pool.ntp.org"             // Primary NTP server
#define NTP_FALLBACK_SERVER "time.google.com" // Fallback NTP server
#define GMT_OFFSET_SEC 7200                      // Set your timezone (e.g., UTC+0)
#define DAYLIGHT_OFFSET_SEC 3600              // Daylight saving time offset (1 hour)
#define TIME_SYNC_INTERVAL_MS 3600000         // Resync time every hour (3600000 ms)
#define TIME_TASK_INTERVAL_MS 15000           // Check time status every 15 seconds

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
    bool isAdvanceNotice;  // Whether this is a 1-minute advance notice
} MedicationReminder;

// Audio Command
typedef struct {
    int fileNumber;
    int repeatCount;
    int volume;
} AudioCommand;

// WiFi Status Structure
typedef struct {
    bool connected;
    int rssi;
    char ip[16];
    unsigned long lastConnectAttempt;
    int failureCount;
    unsigned long lastStatusCheck;
} WiFiStatus;

// Time Synchronization Structure
typedef struct {
    bool synchronized;
    unsigned long lastSyncTimestamp;
    time_t currentEpoch;
    char timeString[32];
    unsigned long lastCheck;
} TimeStatus;

#endif // CONFIG_H
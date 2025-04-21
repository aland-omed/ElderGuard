/**
 * ElderGuard - Main Application
 * 
 * This file initializes the ESP32 with FreeRTOS and creates all required tasks
 * for the ElderGuard elderly monitoring system.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <Wire.h>
#include <WiFi.h>

// Include all task headers
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/ecg_task.h"
#include "../include/gps_task.h"
#include "../include/fall_detection_task.h"
#include "../include/screen_task.h"
#include "../include/audio_task.h"
#include "../include/medication_task.h"
#include "../include/wifi_task.h"
#include "../include/time_task.h"
#include "../include/mqtt_task.h"
#include "../include/http_task.h"

// Task handles
TaskHandle_t ecgTaskHandle = NULL;
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t fallDetectionTaskHandle = NULL;
TaskHandle_t screenTaskHandle = NULL;
TaskHandle_t audioTaskHandle = NULL;
TaskHandle_t medicationTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;
TaskHandle_t timeTaskHandle = NULL;
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t httpTaskHandle = NULL;

// Function declarations
void initHardware();

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(500); // Short delay to let serial initialize
  
  Serial.println("\n\n===== ElderGuard System Initializing =====");
  
  // Initialize hardware components and pins
  initHardware();
  
  // Create mutexes and semaphores
  displayMutex = xSemaphoreCreateMutex();
  ecgDataSemaphore = xSemaphoreCreateBinary();
  gpsDataSemaphore = xSemaphoreCreateBinary();
  fallDetectionSemaphore = xSemaphoreCreateBinary();
  medicationSemaphore = xSemaphoreCreateBinary();
  audioCommandSemaphore = xSemaphoreCreateBinary();
  wifiStatusSemaphore = xSemaphoreCreateBinary();
  timeStatusSemaphore = xSemaphoreCreateBinary();
  
  // Do NOT configure Watchdog timer as requested by the user
  
  // ===== CORE 0 TASKS (Network & Communication) =====
  
  // Create WiFi task first to establish connectivity - HIGHEST PRIORITY ON CORE 0
  xTaskCreatePinnedToCore(
    wifiTask,               // Task function
    "WiFi",                 // Name 
    8192,                   // Stack size (bytes)
    NULL,                   // Parameters
    10,                     // Priority (highest on core 0)
    &wifiTaskHandle,        // Task handle
    0                       // Core (0=Protocol core is better for network tasks)
  );
  
  // Give WiFi task time to initialize before starting other network tasks
  vTaskDelay(500 / portTICK_PERIOD_MS);
  
  // Create time task next to ensure time synchronization
  xTaskCreatePinnedToCore(
    timeTask,               // Task function
    "Time",                 // Name 
    4096,                   // Stack size (bytes)
    NULL,                   // Parameters
    8,                      // Priority (high)
    &timeTaskHandle,        // Task handle
    0                       // Core (0=Protocol core)
  );
  
  // Create MQTT task for data publishing
  xTaskCreatePinnedToCore(
    mqttTask,               // Task function
    "MQTT",                 // Name 
    8192,                   // Stack size (bytes)
    NULL,                   // Parameters
    6,                      // Priority (medium-high)
    &mqttTaskHandle,        // Task handle
    0                       // Core (0=Protocol core)
  );

  // Create HTTP task for data uploading to server
  xTaskCreatePinnedToCore(
    httpTask,               // Task function
    "HTTP",                 // Name 
    8192,                   // Stack size (bytes)
    NULL,                   // Parameters
    4,                      // Priority (medium)
    &httpTaskHandle,        // Task handle
    0                       // Core (0=Protocol core)
  );
  
  // ===== CORE 1 TASKS (Sensors & User Interface) =====
  
  // Fall detection task has the highest priority as it's safety-critical
  xTaskCreatePinnedToCore(
    fallDetectionTask,      // Task function
    "FallDetection",        // Name 
    4096,                   // Stack size (bytes)
    NULL,                   // Parameters
    10,                     // Priority (highest on core 1)
    &fallDetectionTaskHandle, // Task handle
    1                       // Core (1=Application core)
  );
  
  // ECG task has high priority for heart rate monitoring
  xTaskCreatePinnedToCore(
    ecgTask,                // Task function
    "ECG",                  // Name
    4096,                   // Stack size 
    NULL,                   // Parameters
    9,                      // Priority (very high)
    &ecgTaskHandle,         // Task handle
    1                       // Core (1=Application core)
  );
  
  // GPS task has high priority for location services
  xTaskCreatePinnedToCore(
    gpsTask,                // Task function
    "GPS",                  // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    8,                      // Priority (high)
    &gpsTaskHandle,         // Task handle
    1                       // Core (1=Application core)
  );
  
  // Audio task for alerts
  xTaskCreatePinnedToCore(
    audioTask,              // Task function
    "Audio",                // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    7,                      // Priority (medium-high)
    &audioTaskHandle,       // Task handle
    1                       // Core (1=Application core)
  );
  
  // Screen task for display updates
  xTaskCreatePinnedToCore(
    screenTask,             // Task function
    "Screen",               // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    5,                      // Priority (medium)
    &screenTaskHandle,      // Task handle
    1                       // Core (1=Application core)
  );
  
  // Medication task for reminders
  xTaskCreatePinnedToCore(
    medicationTask,         // Task function
    "Medication",           // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    4,                      // Priority (medium-low)
    &medicationTaskHandle,  // Task handle
    1                       // Core (1=Application core)
  );
  
  Serial.println("All tasks started successfully");
  Serial.println("===== ElderGuard System Running =====");
}

void initHardware() {
  // Initialize all hardware pins and peripherals
  
  // I2C Setup for MPU6050 and OLED
  Wire.begin(21, 22);
  
  // Setup AD8232 (ECG) pin
  pinMode(ECG_PIN, INPUT);
  
  // GPS UART
  Serial2.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  
  // MP3 Player UART
  Serial1.begin(9600, SERIAL_8N1, MP3_RX_PIN, MP3_TX_PIN);
  
  Serial.println("Hardware initialization complete");
}

void loop() {
  // The main loop remains mostly empty as tasks handle all the work
  // Just add a small delay to prevent watchdog timer issues
  delay(1000);
}
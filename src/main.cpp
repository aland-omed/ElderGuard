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
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
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
  
  // Configure Watchdog timer for the entire system (optional)
  // esp_task_wdt_init(30, false); // 30 second timeout, no panic
  
  // ===== CORE 0 TASKS (Network & Communication) =====
  // Create WiFi task first to establish connectivity
  xTaskCreatePinnedToCore(
    wifiTask,               // Task function
    "WiFi",                 // Name 
    8192,                   // Stack size (bytes) - INCREASED from 4096
    NULL,                   // Parameters
    10,                     // Priority (very high - needed for other services)
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
    9,                      // Priority (high - needed for timestamping)
    &timeTaskHandle,        // Task handle
    0                       // Core (0=Protocol core)
  );
  
  // Create MQTT task for data publishing
  xTaskCreatePinnedToCore(
    mqttTask,               // Task function
    "MQTT",                 // Name 
    8192,                   // Stack size (bytes) - INCREASED from 4096
    NULL,                   // Parameters
    7,                      // Priority (adjusted down slightly)
    &mqttTaskHandle,        // Task handle
    0                       // Core (0=Protocol core is better for network tasks)
  );

  // Create HTTP task for data uploading to Laravel and Telegram alerts
  xTaskCreatePinnedToCore(
    httpTask,               // Task function
    "HTTP",                 // Name 
    8192,                   // Stack size (bytes) - same as before
    NULL,                   // Parameters
    6,                      // Priority (adjusted down slightly)
    &httpTaskHandle,        // Task handle
    0                       // Core (0=Protocol core is better for network tasks)
  );
  
  // ===== CORE 1 TASKS (Sensors & User Interface) =====
  // Create tasks with appropriate priorities
  // Higher number means higher priority
  xTaskCreatePinnedToCore(
    fallDetectionTask,      // Task function
    "FallDetection",        // Name 
    4096,                   // Stack size (bytes)
    NULL,                   // Parameters
    5,                      // Priority (highest - critical response)
    &fallDetectionTaskHandle, // Task handle
    1                       // Core (1=Application core)
  );
  
  xTaskCreatePinnedToCore(
    ecgTask,                // Task function
    "ECG",                  // Name
    4096,                   // Stack size 
    NULL,                   // Parameters
    5,                      // Priority (high - health monitoring)
    &ecgTaskHandle,         // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    gpsTask,                // Task function
    "GPS",                  // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    5,                      // Priority (medium)
    &gpsTaskHandle,         // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    screenTask,             // Task function
    "Screen",               // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    5,                      // Priority (lower)
    &screenTaskHandle,      // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    audioTask,              // Task function
    "Audio",                // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    5,                      // Priority (medium)
    &audioTaskHandle,       // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    medicationTask,         // Task function
    "Medication",           // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    5,                      // Priority (lower)
    &medicationTaskHandle,  // Task handle
    1                       // Core
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

  // Optional: Monitor system health if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    // We don't need to call mqttClient.loop() here, as it's handled in the MQTT task
  }
}
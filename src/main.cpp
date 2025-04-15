/**
 * ElderGuard - Main Application
 * 
 * This file initializes the ESP32 with FreeRTOS and creates all required tasks
 * for the ElderGuard elderly monitoring system.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <Wire.h>

// Include all task headers
#include "../include/config.h"
#include "../include/ecg_task.h"
#include "../include/gps_task.h"
#include "../include/fall_detection_task.h"
#include "../include/screen_task.h"
#include "../include/audio_task.h"
#include "../include/medication_task.h"

// Task handles
TaskHandle_t ecgTaskHandle = NULL;
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t fallDetectionTaskHandle = NULL;
TaskHandle_t screenTaskHandle = NULL;
TaskHandle_t audioTaskHandle = NULL;
TaskHandle_t medicationTaskHandle = NULL;

// Shared data structures and queues
QueueHandle_t ecgDataQueue;
QueueHandle_t gpsDataQueue;
QueueHandle_t fallDetectionQueue;
QueueHandle_t medicationQueue;
QueueHandle_t audioCommandQueue;

// Mutex for shared resources
SemaphoreHandle_t displayMutex;

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
  
  // Create queues for inter-task communication
  ecgDataQueue = xQueueCreate(10, sizeof(EcgData));
  gpsDataQueue = xQueueCreate(5, sizeof(GpsData));
  fallDetectionQueue = xQueueCreate(3, sizeof(FallEvent));
  medicationQueue = xQueueCreate(5, sizeof(MedicationReminder));
  audioCommandQueue = xQueueCreate(5, sizeof(AudioCommand));
  
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
    4,                      // Priority (high - health monitoring)
    &ecgTaskHandle,         // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    gpsTask,                // Task function
    "GPS",                  // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    3,                      // Priority (medium)
    &gpsTaskHandle,         // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    screenTask,             // Task function
    "Screen",               // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    2,                      // Priority (lower)
    &screenTaskHandle,      // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    audioTask,              // Task function
    "Audio",                // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    3,                      // Priority (medium)
    &audioTaskHandle,       // Task handle
    1                       // Core
  );
  
  xTaskCreatePinnedToCore(
    medicationTask,         // Task function
    "Medication",           // Name
    4096,                   // Stack size
    NULL,                   // Parameters
    2,                      // Priority (lower)
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
  
  // Optional: Print task statistics periodically
  // vTaskGetRunTimeStats()
}
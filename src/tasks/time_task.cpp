/**
 * ElderGuard - Time Management Task Implementation
 * 
 * This file implements the time synchronization functionality that handles
 * syncing with NTP servers and providing accurate time to other components.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <WiFi.h>
#include "../include/time_task.h"
#include "../include/wifi_task.h"
#include "../include/config.h"
#include "../include/globals.h"

void timeTask(void *pvParameters) {
  Serial.println("Time Task: Started");
  
  // Initialize time status
  currentTimeStatus.synchronized = false;
  currentTimeStatus.lastSyncTimestamp = 0;
  currentTimeStatus.currentEpoch = 0;
  
  // Use a temporary non-volatile buffer for string operations
  char buffer[32];
  strcpy(buffer, "Not synchronized");
  memcpy((void*)currentTimeStatus.timeString, buffer, strlen(buffer) + 1);
  
  currentTimeStatus.lastCheck = 0;
  
  // Wait for WiFi connection before attempting NTP sync
  Serial.println("Time Task: Waiting for WiFi connection");
  while (!getWiFiConnected()) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  // Setup time synchronization
  if (setupTimeSync(&currentTimeStatus)) {
    Serial.println("Time Task: Time synchronization initialized successfully");
  } else {
    Serial.println("Time Task: Failed to initialize time synchronization");
  }
  
  // Main task loop
  while (true) {
    // Check if time needs to be synced
    if (millis() - currentTimeStatus.lastSyncTimestamp > TIME_SYNC_INTERVAL_MS) {
      // Only attempt sync if WiFi is connected
      if (getWiFiConnected()) {
        syncTimeWithNTP(&currentTimeStatus);
      } else {
        Serial.println("Time Task: Cannot sync time - WiFi not connected");
      }
    }
    
    // Update current time information
    updateCurrentTime(&currentTimeStatus);
    
    // Signal other tasks that time status is updated
    xSemaphoreGive(timeStatusSemaphore);
    
    // Delay before next check
    vTaskDelay(pdMS_TO_TICKS(TIME_TASK_INTERVAL_MS));
  }
}

bool setupTimeSync(volatile TimeStatus *status) {
  Serial.println("Time Task: Setting up time synchronization");
  
  // Configure NTP time synchronization
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER, NTP_FALLBACK_SERVER);
  
  // Attempt initial sync
  return syncTimeWithNTP(status);
}

bool syncTimeWithNTP(volatile TimeStatus *status) {
  Serial.println("Time Task: Synchronizing time with NTP server");
  
  // Record sync attempt time
  unsigned long syncAttemptStart = millis();
  
  // Set maximum sync timeout
  const unsigned long SYNC_TIMEOUT_MS = 10000; // 10 seconds
  
  // Wait for time to be set up (or timeout)
  time_t now = 0;
  struct tm timeinfo = {0};
  
  while (timeinfo.tm_year < (2020 - 1900) && millis() - syncAttemptStart < SYNC_TIMEOUT_MS) {
    // Get current time
    time(&now);
    localtime_r(&now, &timeinfo);
    delay(100);
  }
  
  // Check if sync was successful
  if (timeinfo.tm_year >= (2020 - 1900)) {
    // Update status
    status->synchronized = true;
    status->lastSyncTimestamp = millis();
    status->currentEpoch = now;
    
    // Format time string
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // Use memcpy for volatile destination
    memcpy((void*)status->timeString, timeStr, strlen(timeStr) + 1);
    
    // Update shared time status with mutex protection
    xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100));
    timeStatusUpdated = true;
    xSemaphoreGive(displayMutex);
    
    Serial.print("Time Task: Time synchronized: ");
    
    // Copy volatile data to temporary buffer for printing
    char timeBuffer[32];
    memcpy(timeBuffer, (const void*)status->timeString, sizeof(timeBuffer));
    Serial.println(timeBuffer);
    
    return true;
  } else {
    Serial.println("Time Task: Failed to synchronize time with NTP server");
    return false;
  }
}

void updateCurrentTime(volatile TimeStatus *status) {
  // Only update periodically to reduce overhead
  if (millis() - status->lastCheck < 1000) {
    return;
  }
  
  status->lastCheck = millis();
  
  // Get current time
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  
  // Update status
  status->currentEpoch = now;
  
  // Format time string
  char timeStr[32];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  
  // Copy volatile data to temporary buffer for comparison
  char currentTimeStr[32];
  memcpy(currentTimeStr, (const void*)status->timeString, sizeof(currentTimeStr));
  
  // Only update shared status if time string has changed
  if (strcmp(currentTimeStr, timeStr) != 0) {
    // Use memcpy for volatile destination
    memcpy((void*)status->timeString, timeStr, strlen(timeStr) + 1);
    
    // Update shared time status with mutex protection
    xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100));
    timeStatusUpdated = true;
    xSemaphoreGive(displayMutex);
  }
}

time_t getCurrentEpochTime() {
  if (!currentTimeStatus.synchronized) {
    Serial.println("Time Task: Warning - Getting time before synchronization");
  }
  return currentTimeStatus.currentEpoch;
}

char* getCurrentTimeString(char* buffer, size_t bufferSize, const char* format) {
  time_t now = currentTimeStatus.currentEpoch;
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  if (format == NULL) {
    format = "%Y-%m-%d %H:%M:%S"; // Default format if none provided
  }
  
  strftime(buffer, bufferSize, format, &timeinfo);
  return buffer;
}

bool isTimeSynchronized() {
  return currentTimeStatus.synchronized;
}
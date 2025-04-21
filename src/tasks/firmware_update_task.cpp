/**
 * ElderGuard - Firmware Update Task Implementation
 * 
 * This file implements the OTA firmware update functionality
 * that connects to the Laravel backend to check for and install updates.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include "../../include/firmware_update_task.h"
#include "../../include/config.h"
#include "../../include/globals.h"
#include "../../include/wifi_task.h"

// Last time we checked for an update
unsigned long lastUpdateCheck = 0;

/**
 * Main firmware update task
 */
void firmwareUpdateTask(void *pvParameters) {
  Serial.println("Firmware Update Task: Started");
  Serial.printf("Current firmware version: %s\n", FIRMWARE_VERSION);
  Serial.printf("Free heap before update check: %d\n", ESP.getFreeHeap());
  
  // Wait a bit before first check to ensure system is stable
  vTaskDelay(pdMS_TO_TICKS(60000)); // Wait 1 minute after boot
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Check for updates periodically
    if (currentTime - lastUpdateCheck >= FIRMWARE_UPDATE_CHECK_INTERVAL || lastUpdateCheck == 0) {
      lastUpdateCheck = currentTime;
      
      // Only check if WiFi is connected
      if (getWiFiConnected()) {
        Serial.println("Firmware Update Task: Checking for updates...");
        
        // Debug memory before update check
        Serial.printf("Free heap before update check: %d\n", ESP.getFreeHeap());
        
        if (checkFirmwareUpdate()) {
          Serial.println("Firmware Update Task: Update complete, restarting device");
          // After successful update, device will restart automatically
        }
      } else {
        Serial.println("Firmware Update Task: WiFi not connected, skipping update check");
      }
    }
    
    // Sleep for a while but wake up periodically to prevent watchdog triggers
    for (int i = 0; i < 360; i++) {  // 3600 seconds (1 hour) in 10-second increments
      vTaskDelay(pdMS_TO_TICKS(10000));  // 10 seconds delay
    }
  }
}

/**
 * Check and perform firmware update if available
 * 
 * @return true if update was applied, false otherwise
 */
bool checkFirmwareUpdate() {
  // Initialize a secure client
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    Serial.println("Firmware Update Task: Unable to create secure client");
    return false;
  }
  
  // Skip certificate verification to avoid SSL issues
  client->setInsecure();
  
  // Set a shorter timeout to avoid watchdog issues
  client->setTimeout(10);  // 10 seconds timeout
  
  HTTPClient http;
  
  // Print the URL for debugging
  Serial.print("Firmware Update Task: Checking URL: ");
  Serial.println(FIRMWARE_API_URL);
  
  // Set up the request with secure client
  if (!http.begin(*client, FIRMWARE_API_URL)) {
    Serial.println("Firmware Update Task: Failed to connect to server with primary URL");
    
    // Try alternative URL format (with /api prefix)
    const char* alternateUrl = "https://elderguard.codecommerce.info/api/firmware";
    Serial.print("Firmware Update Task: Trying alternate URL: ");
    Serial.println(alternateUrl);
    
    if (!http.begin(*client, alternateUrl)) {
      Serial.println("Firmware Update Task: Failed to connect to server with alternate URL");
      delete client;
      return false;
    }
  }
  
  // Set shorter timeout for HTTP operations
  http.setTimeout(15000);  // Increase timeout to 15 seconds
  
  // Get device ID as string - Convert integer PATIENT_ID to string
  String deviceId = String(PATIENT_ID);
  
  // Set headers to identify the device and current version
  http.addHeader("X-Device-ID", deviceId);
  http.addHeader("X-Current-Version", FIRMWARE_VERSION);
  
  Serial.println("Firmware Update Task: Sending request with headers:");
  Serial.printf("X-Device-ID: %s\n", deviceId.c_str());
  Serial.printf("X-Current-Version: %s\n", FIRMWARE_VERSION);
  
  // Print the current URL we're using (fixed getURL error)
  Serial.print("Firmware Update Task: Request URL: ");
  Serial.println(FIRMWARE_API_URL);
  
  // Yield before making the request to ensure watchdog is fed
  vTaskDelay(pdMS_TO_TICKS(10));
  
  // Make the request
  int httpCode = http.GET();
  
  // Yield again after the request
  vTaskDelay(pdMS_TO_TICKS(10));
  
  Serial.printf("Firmware Update Task: HTTP response code: %d\n", httpCode);
  
  if (httpCode < 0) {
    Serial.printf("Firmware Update Task: HTTP error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    delete client;
    return false;
  }
  
  // Check if the request was successful
  if (httpCode == 200) {
    // Get the content length
    int contentLength = http.getSize();
    
    if (contentLength <= 0) {
      Serial.println("Firmware Update Task: No update available");
      http.end();
      delete client;
      return false;
    }
    
    Serial.printf("Firmware Update Task: Firmware update found (%d bytes), starting update\n", contentLength);
    
    // Check if we have enough space
    if (ESP.getFreeSketchSpace() < contentLength) {
      Serial.printf("Firmware Update Task: Not enough space for update. Needed: %d, available: %d\n", 
                   contentLength, ESP.getFreeSketchSpace());
      http.end();
      delete client;
      return false;
    }
    
    // Begin the update process
    if (!Update.begin(contentLength)) {
      Serial.printf("Firmware Update Task: Failed to begin update: %d\n", Update.getError());
      http.end();
      delete client;
      return false;
    }
    
    // Create a buffer for the data
    uint8_t *buffer = (uint8_t*)malloc(1024);
    if (!buffer) {
      Serial.println("Firmware Update Task: Failed to allocate buffer");
      Update.abort();
      http.end();
      delete client;
      return false;
    }
    
    WiFiClient *stream = http.getStreamPtr();
    
    // Read and write the data in chunks
    size_t totalRead = 0;
    unsigned long lastYield = millis();
    
    while (http.connected() && totalRead < contentLength) {
      // Periodically yield to prevent watchdog timer from triggering
      if (millis() - lastYield > 500) {  // Yield every 500ms
        vTaskDelay(pdMS_TO_TICKS(1));
        lastYield = millis();
      }
      
      // Get available data size
      size_t available = stream->available();
      
      if (available) {
        // Read up to the buffer size, but not more than 2K at once to prevent watchdog issues
        size_t readBytes = stream->readBytes(buffer, min(available, (size_t)1024)); 
        
        // Write the data to flash
        size_t written = Update.write(buffer, readBytes);
        if (written != readBytes) {
          Serial.printf("Firmware Update Task: Update write failed: %d\n", Update.getError());
          free(buffer);
          Update.abort();
          http.end();
          delete client;
          return false;
        }
        
        totalRead += readBytes;
        
        // Print progress
        if (totalRead % 10240 == 0) { // Print every 10KB
          Serial.printf("Firmware Update Task: Downloaded %d bytes of %d\n", totalRead, contentLength);
          // Extra yield during progress updates
          vTaskDelay(pdMS_TO_TICKS(10));
        }
      } else {
        // If no data available, yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    
    // Free the buffer
    free(buffer);
    
    Serial.printf("Firmware Update Task: Download complete, %d bytes transferred\n", totalRead);
    
    // Yield before finishing up
    vTaskDelay(pdMS_TO_TICKS(20));
    
    http.end();
    delete client;
    
    // Check if the entire update was completed
    if (totalRead < contentLength) {
      Serial.println("Firmware Update Task: Update incomplete");
      Update.abort();
      return false;
    }
    
    // End the update process
    if (!Update.end(true)) {
      Serial.printf("Firmware Update Task: Update failed with error: %d\n", Update.getError());
      return false;
    }
    
    Serial.println("Firmware Update Task: Update successful!");
    
    // Report success to the server
    reportUpdateStatus(FIRMWARE_VERSION, "success");
    
    Serial.println("Firmware Update Task: Update complete, restarting...");
    delay(1000);
    ESP.restart();
    
    return true;
  } else if (httpCode == 304) {
    Serial.println("Firmware Update Task: Device already has the latest firmware");
    http.end();
    delete client;
    return false;
  } else {
    Serial.printf("Firmware Update Task: Update check failed, HTTP code: %d\n", httpCode);
    
    // Try to get response content for more details
    String payload = http.getString();
    if (payload.length() > 0) {
      Serial.printf("Firmware Update Task: Response: %s\n", payload.c_str());
    }
    
    http.end();
    delete client;
    return false;
  }
}

/**
 * Report update status to the server
 * 
 * @param version The firmware version
 * @param status "success" or "failed"
 */
void reportUpdateStatus(const char* version, const char* status) {
  // Initialize a secure client
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    Serial.println("Firmware Update Task: Unable to create secure client for status report");
    return;
  }
  
  // Skip certificate verification to avoid SSL issues
  client->setInsecure();
  
  // Set a shorter timeout to avoid watchdog issues
  client->setTimeout(10);  // 10 seconds timeout
  
  HTTPClient http;
  
  Serial.print("Firmware Update Task: Reporting status to URL: ");
  Serial.println(FIRMWARE_REPORT_URL);
  
  if (!http.begin(*client, FIRMWARE_REPORT_URL)) {
    Serial.println("Firmware Update Task: Failed to connect to server for status report");
    delete client;
    return;
  }
  
  http.setTimeout(10000);  // 10 seconds timeout
  http.addHeader("Content-Type", "application/json");
  
  // Convert integer PATIENT_ID to string
  String deviceId = String(PATIENT_ID);
  
  // Create JSON payload
  String payload = "{";
  payload += "\"device_id\":\"" + deviceId + "\",";
  payload += "\"version\":\"" + String(version) + "\",";
  payload += "\"status\":\"" + String(status) + "\"";
  payload += "}";
  
  Serial.printf("Firmware Update Task: Sending payload: %s\n", payload.c_str());
  
  // Yield before making the request to ensure watchdog is fed
  vTaskDelay(pdMS_TO_TICKS(10));
  
  // Send the report
  int httpCode = http.POST(payload);
  
  // Yield after the request
  vTaskDelay(pdMS_TO_TICKS(10));
  
  Serial.printf("Firmware Update Task: Status report HTTP code: %d\n", httpCode);
  
  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("Firmware Update Task: Status reported successfully");
  } else {
    Serial.printf("Firmware Update Task: Status report failed, HTTP code: %d\n", httpCode);
    
    // Try to get response content for more details
    String response = http.getString();
    if (response.length() > 0) {
      Serial.printf("Firmware Update Task: Response: %s\n", response.c_str());
    }
  }
  
  http.end();
  delete client;
}
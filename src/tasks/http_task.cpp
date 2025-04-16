/**
 * ElderGuard - Simplified HTTP Task Implementation
 * 
 * This file implements a simplified HTTP client that sends
 * essential sensor data (heart rate, ECG, location) to the Laravel backend.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../include/http_task.h"
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/wifi_task.h"

// HTTP request settings
#define HTTP_MAX_RETRIES 3
#define HTTP_RETRY_DELAY_MS 2000
#define HTTP_PUBLISH_INTERVAL_MS 30000 // 30 seconds between data uploads

// Laravel API endpoints
const char* LARAVEL_API_URL = "http://your-laravel-url.com/api"; // Replace with your actual Laravel URL
const char* SENSOR_DATA_ENDPOINT = "/sensor-data";
const char* ALERT_ENDPOINT = "/alerts";

// Device identification
const char* DEVICE_ID = "ELDERGUARD_001"; // Should match a device_id in your devices table
int PATIENT_ID = 1; // Should match the patient_id in your patients table

// Variables to track last send time
unsigned long lastSensorDataSend = 0;
unsigned long lastHeartRateAlertTime = 0;
unsigned long lastLocationUpdateTime = 0;

void httpTask(void *pvParameters) {
  Serial.println("HTTP Task: Started");
  
  // Wait for WiFi to be connected before proceeding
  while (!getWiFiConnected()) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.println("HTTP Task: Waiting for WiFi connection...");
  }
  
  Serial.println("HTTP Task: WiFi connected, ready to send data");
  
  // Main task loop
  while (true) {
    unsigned long currentTime = millis();
    
    // Only proceed if WiFi is connected
    if (getWiFiConnected()) {
      
      // Send sensor data at regular intervals
      if (currentTime - lastSensorDataSend >= HTTP_PUBLISH_INTERVAL_MS) {
        sendSensorData();
        lastSensorDataSend = currentTime;
      }
      
      // Check for heart rate alerts
      if (xSemaphoreTake(ecgDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
        int heartRate = currentEcgData.heartRate;
        bool validSignal = currentEcgData.validSignal;
        
        // Only alert if we have a valid signal and if enough time has passed since last alert
        if (validSignal && currentTime - lastHeartRateAlertTime > 60000) { // 1 minute cooldown
          if (heartRate > 0 && (heartRate > 120)) { // Only high heart rate alerts for simplicity
            sendHeartRateAlert(heartRate);
            lastHeartRateAlertTime = currentTime;
          }
        }
        
        xSemaphoreGive(ecgDataSemaphore);
      }
      
      // Check for location updates and safe zone violations
      if (xSemaphoreTake(gpsDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (currentGpsData.validFix && (currentTime - lastLocationUpdateTime > 60000)) {
          sendLocationData();
          lastLocationUpdateTime = currentTime;
        }
        
        xSemaphoreGive(gpsDataSemaphore);
      }
    } else {
      Serial.println("HTTP Task: WiFi not connected, skipping data upload");
    }
    
    // Delay before next check
    vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
  }
}

void sendSensorData() {
  // Create a JSON document for the sensor data
  StaticJsonDocument<512> doc;

  // Take semaphores to access shared data
  bool hasValidData = false;
  
  // Add heart rate and ECG data
  if (xSemaphoreTake(ecgDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (currentEcgData.validSignal) {
      doc["patient_id"] = PATIENT_ID;
      doc["heart_rate"] = currentEcgData.heartRate;
      
      // Create a JSON array for ECG data (last 100 samples)
      JsonArray ecgArray = doc.createNestedArray("ecg_data");
      for (int i = 0; i < 100; i++) {
        // In a real implementation, you would have a buffer of ECG samples
        // Here we just use the current value as a placeholder
        ecgArray.add(currentEcgData.rawValue);
      }
      
      hasValidData = true;
    }
    xSemaphoreGive(ecgDataSemaphore);
  }
  
  // Add location data
  if (xSemaphoreTake(gpsDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (currentGpsData.validFix) {
      JsonObject location = doc.createNestedObject("location");
      location["latitude"] = currentGpsData.latitude;
      location["longitude"] = currentGpsData.longitude;
      hasValidData = true;
    }
    xSemaphoreGive(gpsDataSemaphore);
  }
  
  // Only send the data if we have valid data
  if (hasValidData) {
    String jsonData;
    serializeJson(doc, jsonData);
    
    HTTPClient http;
    String url = String(LARAVEL_API_URL) + String(SENSOR_DATA_ENDPOINT);
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Implement retry logic
    bool success = false;
    int retries = 0;
    
    while (!success && retries < HTTP_MAX_RETRIES) {
      // Send the POST request
      int httpResponseCode = http.POST(jsonData);
      
      // Check response
      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        Serial.printf("HTTP Task: Sensor data sent successfully, response code: %d\n", httpResponseCode);
        success = true;
      } else {
        Serial.printf("HTTP Task: Failed to send sensor data, error: %s (attempt %d/%d)\n", 
                     http.errorToString(httpResponseCode).c_str(), retries + 1, HTTP_MAX_RETRIES);
        retries++;
        
        if (retries < HTTP_MAX_RETRIES) {
          vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS)); // Wait before retrying
        }
      }
    }
    
    http.end();
  } else {
    Serial.println("HTTP Task: No valid sensor data to send");
  }
}

void sendHeartRateAlert(int heartRate) {
  // Create JSON document for alert
  StaticJsonDocument<256> doc;
  doc["patient_id"] = PATIENT_ID;
  doc["alert_type"] = "high_heart_rate";
  doc["message"] = "⚠️ High heart rate detected: " + String(heartRate) + " BPM";
  
  // Serialize the JSON to string
  String jsonData;
  serializeJson(doc, jsonData);
  
  // Create HTTP client and set headers
  HTTPClient http;
  String url = String(LARAVEL_API_URL) + String(ALERT_ENDPOINT);
  
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  // Send the alert with retry logic
  bool success = false;
  int retries = 0;
  
  while (!success && retries < HTTP_MAX_RETRIES) {
    int httpResponseCode = http.POST(jsonData);
    
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      Serial.printf("HTTP Task: Heart rate alert sent, response code: %d\n", httpResponseCode);
      success = true;
    } else {
      Serial.printf("HTTP Task: Failed to send heart rate alert, error: %s (attempt %d/%d)\n", 
                   http.errorToString(httpResponseCode).c_str(), retries + 1, HTTP_MAX_RETRIES);
      retries++;
      
      if (retries < HTTP_MAX_RETRIES) {
        vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
      }
    }
  }
  
  http.end();
  
  Serial.printf("HTTP Task: Heart rate alert sent for: %d BPM\n", heartRate);
}

void sendLocationData() {
  if (xSemaphoreTake(gpsDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (!currentGpsData.validFix) {
      xSemaphoreGive(gpsDataSemaphore);
      return;
    }
    
    // Create JSON for location data
    StaticJsonDocument<256> doc;
    doc["patient_id"] = PATIENT_ID;
    doc["latitude"] = currentGpsData.latitude;
    doc["longitude"] = currentGpsData.longitude;
    
    // Serialize JSON
    String jsonData;
    serializeJson(doc, jsonData);
    
    // Create HTTP client
    HTTPClient http;
    String url = String(LARAVEL_API_URL) + "/location-tracking";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Send with retry logic
    bool success = false;
    int retries = 0;
    
    while (!success && retries < HTTP_MAX_RETRIES) {
      int httpResponseCode = http.POST(jsonData);
      
      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        Serial.printf("HTTP Task: Location data sent, response code: %d\n", httpResponseCode);
        success = true;
      } else {
        Serial.printf("HTTP Task: Failed to send location data, error: %s (attempt %d/%d)\n", 
                     http.errorToString(httpResponseCode).c_str(), retries + 1, HTTP_MAX_RETRIES);
        retries++;
        
        if (retries < HTTP_MAX_RETRIES) {
          vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }
      }
    }
    
    http.end();
    
    xSemaphoreGive(gpsDataSemaphore);
  }
}
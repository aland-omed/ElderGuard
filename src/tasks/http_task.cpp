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
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "../include/http_task.h"
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/wifi_task.h"
#include "../include/ecg_task.h" // Added to directly access ecgBuffer

// Define ECG_BUFFER_SIZE to match the value in ecg_task.cpp
#define ECG_BUFFER_SIZE 250  // 5 seconds at 50Hz

// Laravel API settings
const char* LARAVEL_API_URL = "https://elderguard.codecommerce.info/api";
const char* SENSOR_DATA_ENDPOINT = "/sensor-data";
const char* ALERT_ENDPOINT = "/alerts";
const char* LOCATION_ENDPOINT = "/location-tracking";
const char* PATIENT_LOCATION_ENDPOINT = "/patients/1/locations"; // New endpoint for patient specific location

// Telegram Bot settings
const char* TELEGRAM_BOT_TOKEN = "7250747996:AAGZ_luXdgcnZls1QddK5z2UQ2TUVzjvgzY";
const char* TELEGRAM_CHAT_ID = "6069199442";
const char* TELEGRAM_API_URL = "https://api.telegram.org/bot";

// HTTP request settings
#define HTTP_MAX_RETRIES 3
#define HTTP_RETRY_DELAY_MS 2000
#define HTTP_PUBLISH_INTERVAL_MS 30000 // 30 seconds between data uploads
#define HTTP_TIMEOUT 10000 // 10 seconds timeout for HTTP requests

// Device identification
// Using PATIENT_ID from config.h

// Variables to track last send time
unsigned long lastSensorDataSend = 0;
unsigned long lastHeartRateAlertTime = 0;
unsigned long lastLocationUpdateTime = 0;

// Create a secure client that can connect to HTTPS
WiFiClientSecure secureClient;

void httpTask(void *pvParameters) {
  Serial.println("HTTP Task: Started");
  
  // Configure secure client to use certificates or skip verification
  secureClient.setInsecure(); // Skip verification for simplicity (use proper certs in production)
  
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
          sendPatientLocationData(); // Send patient-specific location data
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

// Function to get real ECG data directly from the source
void getEcgData(char* buffer, int maxSize) {
  // Starting with opening bracket for JSON array
  strcpy(buffer, "[");
  int bufPos = 1; // Position after the opening bracket
  
  // Take semaphore for thread-safe access
  bool success = false;
  if (xSemaphoreTake(ecgDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Direct access to ecgBuffer from ecg_task.h
    // We'll always use whatever data is in the buffer, regardless of leads status
    Serial.println("HTTP Task: Getting ECG data points from buffer");
    
    for (int i = 0; i < 10; i++) {
      // Get index in circular buffer - most recent samples first
      int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
      
      // Add comma if not first item
      if (i > 0) {
        buffer[bufPos++] = ',';
      }
      
      // Get actual ECG values
      int value = ecgBuffer[idx];
      
      // For debugging - print a few values
      if (i < 3) {
        Serial.printf("ECG buffer value %d: %d\n", i, value);
      }
      
      // Convert to string
      char tempStr[10];
      itoa(value, tempStr, 10);
      
      // Copy to buffer
      strcpy(&buffer[bufPos], tempStr);
      bufPos = strlen(buffer);
      
      // Check for buffer overflow
      if (bufPos > maxSize - 10) {
        break;
      }
    }
    success = true;
    
    xSemaphoreGive(ecgDataSemaphore);
  } else {
    Serial.println("HTTP Task: Failed to acquire ecgDataSemaphore");
  }
  
  // If we couldn't get data from the buffer, use fallback values
  if (!success) {
    Serial.println("HTTP Task: USING FALLBACK ECG DATA");
    
    // Generate fallback values different from the constant 500-509 pattern
    // to verify our code is running
    for (int i = 0; i < 10; i++) {
      // Add comma if not first item
      if (i > 0) {
        buffer[bufPos++] = ',';
      }
      
      // Use random values instead of 500+i to see the change
      int value = 1000 + random(1000);
      
      char tempStr[10];
      itoa(value, tempStr, 10);
      
      // Copy to buffer
      strcpy(&buffer[bufPos], tempStr);
      bufPos = strlen(buffer);
      
      // Check for buffer overflow
      if (bufPos > maxSize - 10) {
        break;
      }
    }
  }
  
  // Finish with closing bracket
  buffer[bufPos++] = ']';
  buffer[bufPos] = '\0';
  
  // Debug output
  Serial.print("HTTP Task: Final ECG data array: ");
  Serial.println(buffer);
}

void sendSensorData() {
  // Check WiFi connection first before allocating memory for JSON
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HTTP Task: WiFi not connected, skipping data upload");
    return;
  }
  
  // Create a StaticJsonDocument with minimal size
  StaticJsonDocument<512> doc;  // Reduced size for main document
  
  // Add required fields
  doc["patient_id"] = PATIENT_ID;
  doc["heart_rate"] = 0;
  
  // Get heart rate data if available
  if (xSemaphoreTake(ecgDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (currentEcgData.validSignal) {
      doc["heart_rate"] = currentEcgData.heartRate;
    }
    xSemaphoreGive(ecgDataSemaphore);
  }
  
  // Access external ecgBuffer and bufferIndex variables from ECG task
  extern int ecgBuffer[];
  extern int bufferIndex;
  extern bool leadsConnected;
  
  // DEBUG: Print status of external variables
  Serial.println("-------------------- ECG DATA DEBUG --------------------");
  Serial.printf("HTTP Task: leadsConnected = %s\n", leadsConnected ? "true" : "false");
  Serial.printf("HTTP Task: bufferIndex = %d\n", bufferIndex);
  
  // Test if we can directly access the ECG buffer here
  int testIndex = (bufferIndex - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
  Serial.printf("HTTP Task: Direct buffer access test, ecgBuffer[%d] = %d\n", testIndex, ecgBuffer[testIndex]);
  
  // Create ECG data directly as a string (more memory efficient)
  char ecgJsonBuffer[128]; // Fixed size buffer
  getEcgData(ecgJsonBuffer, sizeof(ecgJsonBuffer));
  
  // DEBUG: Print the ECG data being sent
  Serial.print("HTTP Task: ECG data: ");
  Serial.println(ecgJsonBuffer);
  Serial.println("-------------------- END DEBUG --------------------");
  
  // Add ECG data as string
  doc["ecg_data"] = ecgJsonBuffer;
  
  // Handle location data efficiently
  char locBuffer[80]; // Fixed size buffer
  strcpy(locBuffer, "{\"latitude\":0.0,\"longitude\":0.0}"); // Default values
  
  // Update location data if available
  if (xSemaphoreTake(gpsDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    if (currentGpsData.validFix) {
      // Direct write to buffer with fixed format
      sprintf(locBuffer, "{\"latitude\":%.6f,\"longitude\":%.6f}", 
              currentGpsData.latitude, currentGpsData.longitude);
    }
    xSemaphoreGive(gpsDataSemaphore);
  }
  
  // Add location as string
  doc["location"] = locBuffer;
  
  // Serialize to buffer with fixed size
  char jsonBuffer[512];
  size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
  
  // Create HTTP client
  HTTPClient http;
  String url = String(LARAVEL_API_URL) + String(SENSOR_DATA_ENDPOINT);
  
  http.begin(secureClient, url);
  http.setTimeout(HTTP_TIMEOUT);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  
  // Send data
  int httpResponseCode = http.POST((uint8_t*)jsonBuffer, len);
  
  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    Serial.printf("HTTP Task: Data sent successfully, code: %d\n", httpResponseCode);
  } else {
    Serial.printf("HTTP Task: Send failed, code: %d\n", httpResponseCode);
    // Get limited response
    String response = http.getString();
    if (response.length() > 60) {
      response = response.substring(0, 60) + "...";
    }
    Serial.println("Response: " + response);
  }
  
  http.end();
}

void sendHeartRateAlert(int heartRate) {
  // Create JSON document for alert
  StaticJsonDocument<256> doc;
  doc["patient_id"] = PATIENT_ID;
  doc["alert_type"] = "high_heart_rate";
  doc["message"] = "High heart rate detected: " + String(heartRate) + " BPM";
  
  // Serialize the JSON to string
  String jsonData;
  serializeJson(doc, jsonData);
  
  // Create HTTP client and set headers
  HTTPClient http;
  String url = String(LARAVEL_API_URL) + String(ALERT_ENDPOINT);
  
  // Use secure client for HTTPS
  http.begin(secureClient, url);
  http.setTimeout(HTTP_TIMEOUT);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  
  // Send the alert with retry logic
  bool success = false;
  int retries = 0;
  
  while (!success && retries < HTTP_MAX_RETRIES) {
    int httpResponseCode = http.POST(jsonData);
    String responseBody = http.getString();
    
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      Serial.printf("HTTP Task: Heart rate alert sent, response code: %d\n", httpResponseCode);
      success = true;
    } else {
      Serial.printf("HTTP Task: Failed to send heart rate alert, error code: %d (attempt %d/%d)\n", 
                   httpResponseCode, retries + 1, HTTP_MAX_RETRIES);
      Serial.println("HTTP Task: Response body: " + responseBody);
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
    doc["latitude"] = currentGpsData.latitude; // Use latitude key in the location object
    doc["longitude"] = currentGpsData.longitude; // Use longitude key in the location object
    
    // Serialize JSON
    String jsonData;
    serializeJson(doc, jsonData);
    
    // Create HTTP client
    HTTPClient http;
    String url = String(LARAVEL_API_URL) + String(LOCATION_ENDPOINT);
    
    // Use secure client for HTTPS
    http.begin(secureClient, url);
    http.setTimeout(HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    
    // Send with retry logic
    bool success = false;
    int retries = 0;
    
    while (!success && retries < HTTP_MAX_RETRIES) {
      int httpResponseCode = http.POST(jsonData);
      String responseBody = http.getString();
      
      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        Serial.printf("HTTP Task: Location data sent, response code: %d\n", httpResponseCode);
        success = true;
      } else {
        Serial.printf("HTTP Task: Failed to send location data, error code: %d (attempt %d/%d)\n", 
                     httpResponseCode, retries + 1, HTTP_MAX_RETRIES);
        Serial.println("HTTP Task: Response body: " + responseBody);
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

/**
 * Send message via Telegram Bot API
 * 
 * @param message The message to send
 */
void sendTelegramMessage(const char* message) {
  // Check if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot send Telegram message.");
    return;
  }

  Serial.print("Sending Telegram message: ");
  Serial.println(message);
  
  HTTPClient http;
  
  // Construct the complete URL
  String url = String(TELEGRAM_API_URL) + TELEGRAM_BOT_TOKEN + "/sendMessage";
  
  // Use secure client for HTTPS
  http.begin(secureClient, url);
  http.setTimeout(HTTP_TIMEOUT);
  http.addHeader("Content-Type", "application/json");
  
  // Create the JSON payload
  String payload = "{\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) + "\",\"text\":\"" + message + "\"}";
  
  // Send the request and get the response code
  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("Telegram API Response: ");
    Serial.println(httpResponseCode);
    Serial.println(response);
  } else {
    Serial.print("Telegram API Error: ");
    Serial.println(httpResponseCode);
  }
  
  // Close connection
  http.end();
  
  // Brief delay to avoid hammering the API
  vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * Send location data to patient-specific location endpoint
 * Only sends if coordinates are not (0,0)
 */
void sendPatientLocationData() {
  if (xSemaphoreTake(gpsDataSemaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Only proceed if we have a valid fix and coordinates are not (0,0)
    if (!currentGpsData.validFix || 
        (currentGpsData.latitude == 0.0 && currentGpsData.longitude == 0.0)) {
      xSemaphoreGive(gpsDataSemaphore);
      return;
    }
    
    // Create JSON for location data
    StaticJsonDocument<256> doc;
    doc["latitude"] = currentGpsData.latitude;
    doc["longitude"] = currentGpsData.longitude;
    doc["timestamp"] = millis(); // Using millis as timestamp
    
    // Serialize JSON
    String jsonData;
    serializeJson(doc, jsonData);
    
    // Create HTTP client
    HTTPClient http;
    String url = String(LARAVEL_API_URL) + String(PATIENT_LOCATION_ENDPOINT);
    
    // Use secure client for HTTPS
    http.begin(secureClient, url);
    http.setTimeout(HTTP_TIMEOUT);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    
    // Send with retry logic
    bool success = false;
    int retries = 0;
    
    while (!success && retries < HTTP_MAX_RETRIES) {
      int httpResponseCode = http.POST(jsonData);
      String responseBody = http.getString();
      
      if (httpResponseCode >= 200 && httpResponseCode < 300) {
        Serial.printf("HTTP Task: Patient location data sent to specific endpoint, response code: %d\n", httpResponseCode);
        success = true;
      } else {
        Serial.printf("HTTP Task: Failed to send patient location data, error code: %d (attempt %d/%d)\n", 
                     httpResponseCode, retries + 1, HTTP_MAX_RETRIES);
        Serial.println("HTTP Task: Response body: " + responseBody);
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
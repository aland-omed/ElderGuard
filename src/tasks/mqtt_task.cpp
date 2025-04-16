/**
 * ElderGuard - MQTT Task Implementation
 * 
 * Handles MQTT connectivity and data publishing for the ElderGuard system
 */

#include "../include/mqtt_task.h"
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/wifi_task.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_task_wdt.h"  // Include watchdog timer functions
#include <time.h>

// MQTT Settings - Updated topic to match web frontend expectation
const char* mqtt_server = "test.mosquitto.org"; // Using the same broker as the website
const int mqtt_port = 1883;
const char* mqtt_client_id = "ElderGuard_Device";
const char* mqtt_topic_realtime = "patient/1/realtime";  // Changed to match website's subscription
const char* mqtt_topic_status = "patient/1/status";      // New topic for device status

// MQTT Client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQTT message buffer
char mqttBuffer[512];

// Status update timestamp
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 30000; // 30 seconds

// Forward declaration of publishStatusUpdate
void publishStatusUpdate(bool forceUpdate = false);

void setupMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  // Set a reasonable timeout for MQTT operations to prevent blocking
  espClient.setTimeout(5000); // 5 second timeout
  Serial.println("MQTT client initialized");
}

// Modified reconnectMqtt with better error handling and timeout prevention
void reconnectMqtt() {
  // Always reset the watchdog timer before attempting reconnection
  esp_task_wdt_reset();
  
  // Check if WiFi is connected first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Not attempting MQTT connection");
    // Reset watchdog timer again
    esp_task_wdt_reset();
    // Properly yield to the OS
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    return; // Exit the function instead of blocking
  }
  
  // Only attempt reconnection if not already connected
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Reset watchdog timer before DNS resolution
    esp_task_wdt_reset();
    
    // Attempt to connect with last will testament and timeout
    bool success = mqttClient.connect(mqtt_client_id, NULL, NULL, mqtt_topic_status, 0, true, "{\"status\":\"offline\"}");
    
    // Reset watchdog timer after connection attempt
    esp_task_wdt_reset();
    
    if (success) {
      Serial.println("connected");
      // Publish online status with timestamp
      publishStatusUpdate(true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" will retry later");
    }
  }
  
  // Yield to other tasks
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

// New function to publish device status
void publishStatusUpdate(bool forceUpdate) {
  unsigned long currentTime = millis();
  
  // Only publish status every 30 seconds unless forced
  if (!forceUpdate && (currentTime - lastStatusUpdate < STATUS_UPDATE_INTERVAL)) {
    return;
  }
  
  lastStatusUpdate = currentTime;
  
  // Get current time
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
  
  // Create JSON document for status
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["status"] = "online";
  jsonDoc["created_at"] = timeStr;
  
  // Serialize JSON to buffer
  size_t n = serializeJson(jsonDoc, mqttBuffer);
  
  // Publish status message as retained
  Serial.print("Publishing status update: ");
  Serial.println(mqttBuffer);
  // Fix the type mismatch by casting mqttBuffer to const uint8_t*
  if (mqttClient.publish(mqtt_topic_status, (const uint8_t*)mqttBuffer, n, true)) {
    Serial.println("Status update published successfully");
  } else {
    Serial.println("Failed to publish status update");
  }
}

void publishEcgData(int rawValue, int heartRate, bool validSignal) {
  // Reset watchdog timer before operation
  esp_task_wdt_reset();
  
  // Only publish if connected to both WiFi and MQTT
  if (!getWiFiConnected()) {
    Serial.println("WiFi not connected, not publishing ECG data");
    return;
  }
  
  if (!mqttClient.connected()) {
    Serial.println("MQTT not connected, attempting reconnection");
    reconnectMqtt();
    if (!mqttClient.connected()) {
      Serial.println("Failed to reconnect to MQTT, not publishing ECG data");
      return;
    }
  }

  // Get current time
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // Create JSON document for ECG data
  StaticJsonDocument<512> jsonDoc;
  
  // Add ECG data
  jsonDoc["heart_rate"] = heartRate;
  jsonDoc["valid_signal"] = validSignal ? 1 : 0;
  jsonDoc["created_at"] = timeStr; // Add timestamp
  
  // Create an array for ECG waveform (simplified)
  JsonArray ecgArray = jsonDoc.createNestedArray("ecg_data");
  
  // Reset watchdog timer during potentially long operation
  esp_task_wdt_reset();
  
  // Limit the number of array elements to prevent blocking too long
  int maxPoints = 10;
  for (int i = 0; i < maxPoints; i++) {
    // Here you could use actual ECG data points from a buffer
    // For now, we're just adding the raw value with some variation
    ecgArray.add(rawValue + random(-50, 50));
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(jsonDoc, mqttBuffer);
  
  // Reset watchdog timer before publish
  esp_task_wdt_reset();
  
  // Publish message
  Serial.print("Publishing ECG data: ");
  Serial.println(mqttBuffer);
  if (mqttClient.publish(mqtt_topic_realtime, mqttBuffer, n)) {
    Serial.println("ECG data published successfully");
  } else {
    Serial.println("Failed to publish ECG data");
  }
  
  // Yield to other tasks
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void publishGpsData(float latitude, float longitude) {
  // Reset watchdog timer before operation
  esp_task_wdt_reset();
  
  // Only publish if connected to both WiFi and MQTT
  if (!getWiFiConnected()) {
    Serial.println("WiFi not connected, not publishing GPS data");
    return;
  }
  
  if (!mqttClient.connected()) {
    Serial.println("MQTT not connected, attempting reconnection");
    reconnectMqtt();
    if (!mqttClient.connected()) {
      Serial.println("Failed to reconnect to MQTT, not publishing GPS data");
      return;
    }
  }

  // Get current time
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[30];
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);

  // Create JSON document for location data
  StaticJsonDocument<256> jsonDoc;
  
  // Nest the location data in a location object
  JsonObject locationObj = jsonDoc.createNestedObject("location");
  locationObj["latitude"] = String(latitude, 6);  // Convert to string with 6 decimal places
  locationObj["longitude"] = String(longitude, 6);
  
  // Add timestamp to the main object
  jsonDoc["created_at"] = timeStr;
  
  // Serialize JSON to buffer
  size_t n = serializeJson(jsonDoc, mqttBuffer);
  
  // Reset watchdog timer before publish
  esp_task_wdt_reset();
  
  // Publish message
  Serial.print("Publishing GPS data: ");
  Serial.println(mqttBuffer);
  if (mqttClient.publish(mqtt_topic_realtime, mqttBuffer, n)) {
    Serial.println("GPS data published successfully");
  } else {
    Serial.println("Failed to publish GPS data");
  }
  
  // Yield to other tasks
  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void mqttTask(void *pvParameters) {
  // Register this task with the watchdog timer
  esp_task_wdt_init(30, true); // 30 seconds timeout, panic on timeout
  esp_task_wdt_add(NULL); // Add current task to WDT watch
  
  // Initialize MQTT
  setupMqtt();
  
  Serial.println("MQTT task waiting for WiFi connection...");
  
  // Main task loop
  while (1) {
    // Reset watchdog timer at the beginning of each loop iteration
    esp_task_wdt_reset();
    
    // Wait for WiFi to be connected
    if (!getWiFiConnected()) {
      Serial.println("MQTT task waiting for WiFi connection...");
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      continue; // Skip the rest of the loop
    }
    
    // Ensure MQTT is connected, but with a timeout guard
    if (!mqttClient.connected()) {
      reconnectMqtt();
    } else {
      // Publish regular status updates when connected
      publishStatusUpdate();
    }
    
    // Reset watchdog timer before MQTT loop
    esp_task_wdt_reset();
    
    // Handle MQTT loop with timeout guard to prevent blocking
    unsigned long start = millis();
    mqttClient.loop();
    
    // Check if loop took too long
    if (millis() - start > 1000) {
      Serial.println("Warning: MQTT loop took too long!");
    }
    
    // Always yield to prevent watchdog timer from being triggered
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
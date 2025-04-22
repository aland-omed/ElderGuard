/**
 * ElderGuard - MQTT Task Implementation
 * 
 * Handles MQTT connectivity and data publishing for the ElderGuard system
 * Designed for cooperative multitasking to avoid watchdog timer issues
 */

#include "../include/mqtt_task.h"
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/wifi_task.h"
#include "../include/ecg_task.h" 
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>

// MQTT Settings - using HiveMQ as a reliable free public broker
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* mqtt_client_id = "ElderGuard_Device";

// Topic structure for better organization and filtering
const char* mqtt_topic_realtime = "elderguard/patient/1/realtime";
const char* mqtt_topic_status = "elderguard/patient/1/status";

// MQTT Client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQTT message buffer
char mqttBuffer[512];

// Status update timing
unsigned long lastStatusUpdate = 0;
const unsigned long STATUS_UPDATE_INTERVAL = 30000; // 30 seconds

// Connection retry parameters
unsigned long lastConnectAttempt = 0;
const unsigned long CONNECT_RETRY_INTERVAL = 5000; // 5 seconds between retries

// Debug flag - set to false to disable verbose output
bool mqtt_debug = false;

// Data update intervals (1 second as requested)
const unsigned long DATA_UPDATE_INTERVAL = 1000; 
unsigned long lastEcgUpdate = 0;
unsigned long lastGpsUpdate = 0;
unsigned long lastFallUpdate = 0;

// Forward declarations
bool connectMqttNonBlocking();
bool publishStatusUpdateNonBlocking(bool forceUpdate);
bool publishEcgDataNonBlocking();
bool publishGpsDataNonBlocking();
bool publishFallDataNonBlocking();

/**
 * Initialize MQTT client and settings
 */
void setupMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  espClient.setTimeout(2000); // Short timeout to prevent blocking
  mqttClient.setBufferSize(512);
  
  if (mqtt_debug) {
    Serial.print("MQTT client initialized with broker: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
  }
}

/**
 * Non-blocking MQTT connection attempt
 */
bool connectMqttNonBlocking() {
  unsigned long currentMillis = millis();
  
  // Only retry after interval
  if (currentMillis - lastConnectAttempt < CONNECT_RETRY_INTERVAL) {
    return mqttClient.connected();
  }
  
  lastConnectAttempt = currentMillis;
  
  // Only attempt reconnection if not already connected
  if (mqttClient.connected()) {
    return true;
  }
  
  // Check WiFi first
  if (WiFi.status() != WL_CONNECTED) {
    if (mqtt_debug) Serial.println("WiFi disconnected - skipping MQTT connection");
    return false;
  }
  
  Serial.print("Attempting MQTT connection... ");
  
  // Last will testament message
  StaticJsonDocument<128> lastWillDoc;
  lastWillDoc["status"] = "offline";
  lastWillDoc["device_id"] = mqtt_client_id;
  lastWillDoc["timestamp"] = millis();
  
  char lastWillBuffer[128];
  size_t n = serializeJson(lastWillDoc, lastWillBuffer);
  
  // Set a very short client timeout
  espClient.setTimeout(500);
  
  // Non-blocking connection attempt
  bool result = mqttClient.connect(mqtt_client_id, NULL, NULL, mqtt_topic_status, 0, true, lastWillBuffer);
  
  if (result) {
    Serial.println("connected!");
    return true;
  } else {
    // Don't print detailed errors in normal mode to reduce serial overhead
    if (mqtt_debug) {
      Serial.print("failed, rc=");
      Serial.println(mqttClient.state());
    } else {
      Serial.println("failed");
    }
    return false;
  }
}

/**
 * Non-blocking status update publisher
 */
bool publishStatusUpdateNonBlocking(bool forceUpdate) {
  unsigned long currentMillis = millis();
  
  // Only update periodically unless forced
  if (!forceUpdate && (currentMillis - lastStatusUpdate < STATUS_UPDATE_INTERVAL)) {
    return false;
  }
  
  // Create status JSON document
  StaticJsonDocument<256> statusDoc;
  statusDoc["status"] = "online";
  statusDoc["device_id"] = mqtt_client_id;
  
  // Add timestamp 
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    statusDoc["created_at"] = timeStr;
  } else {
    statusDoc["created_at"] = String(currentMillis);
  }
  
  // Add WiFi information
  JsonObject wifiObj = statusDoc.createNestedObject("wifi");
  wifiObj["rssi"] = currentWiFiStatus.rssi;
  
  char ipBuffer[16];
  strncpy(ipBuffer, (const char*)currentWiFiStatus.ip, sizeof(ipBuffer)-1);
  ipBuffer[sizeof(ipBuffer)-1] = '\0';
  
  wifiObj["ip"] = ipBuffer;
  wifiObj["connected"] = currentWiFiStatus.connected;
  
  // Add sensor status
  JsonObject sensorsObj = statusDoc.createNestedObject("sensors");
  sensorsObj["ecg"] = ecgDataUpdated;
  sensorsObj["gps"] = gpsDataUpdated;
  sensorsObj["fall_detection"] = fallDetectionUpdated;
  
  // Serialize JSON to buffer
  size_t n = serializeJson(statusDoc, mqttBuffer);
  
  // Publish status message (retained)
  if (mqtt_debug) {
    Serial.print("Publishing status: ");
    Serial.println(mqttBuffer);
  }
  
  // Publish with timeout protection
  bool published = mqttClient.publish(mqtt_topic_status, (const uint8_t*)mqttBuffer, n, true);
  
  if (published) {
    lastStatusUpdate = currentMillis;
    if (mqtt_debug) Serial.println("Status published successfully");
  } else {
    if (mqtt_debug) Serial.println("Failed to publish status");
  }
  
  return published;
}

/**
 * Non-blocking ECG data publisher
 */
bool publishEcgDataNonBlocking() {
  unsigned long currentMillis = millis();
  
  // Limit publishing rate to once per second as requested
  if (currentMillis - lastEcgUpdate < DATA_UPDATE_INTERVAL) {
    return false;
  }
  
  // Check if we have updated ECG data
  if (!ecgDataUpdated) {
    return false;
  }
  
  // Create ECG JSON document
  StaticJsonDocument<384> ecgDoc;
  ecgDoc["type"] = "ecg";
  ecgDoc["heart_rate"] = currentEcgData.heartRate;
  ecgDoc["valid_signal"] = currentEcgData.validSignal ? 1 : 0;
  ecgDoc["device_id"] = mqtt_client_id;
  
  // Add timestamp
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ecgDoc["created_at"] = timeStr;
  } else {
    ecgDoc["timestamp"] = millis();
  }
  
  // Create ECG data array
  JsonArray ecgArray = ecgDoc.createNestedArray("ecg_data");
  
  // Try to get ECG data from the buffer
  bool hasRealData = false;
  
  // Use a very short timeout for semaphore (10ms)
  if (xSemaphoreTake(ecgDataSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
    // Add samples to array - use only a small number to reduce processing
    for (int i = 0; i < 10; i++) {
      int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
      ecgArray.add(ecgBuffer[idx]);
    }
    hasRealData = true;
    xSemaphoreGive(ecgDataSemaphore);
  } else {
    if (mqtt_debug) Serial.println("MQTT Task: Failed to acquire ECG semaphore");
  }
  
  // If no real data, skip publishing
  if (!hasRealData) {
    return false;
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(ecgDoc, mqttBuffer);
  
  // Publish ECG data
  bool published = mqttClient.publish(mqtt_topic_realtime, (const uint8_t*)mqttBuffer, n);
  
  if (published) {
    lastEcgUpdate = currentMillis;
    if (mqtt_debug) Serial.println("ECG data published successfully");
  } else if (mqtt_debug) {
    Serial.println("Failed to publish ECG data");
  }
  
  return published;
}

/**
 * Non-blocking GPS data publisher
 */
bool publishGpsDataNonBlocking() {
  unsigned long currentMillis = millis();
  
  // Limit publishing rate to once per second as requested
  if (currentMillis - lastGpsUpdate < DATA_UPDATE_INTERVAL) {
    return false;
  }
  
  // Check if we have updated GPS data
  if (!gpsDataUpdated) {
    return false;
  }
  
  // Create GPS JSON document
  StaticJsonDocument<256> gpsDoc;
  gpsDoc["type"] = "gps";
  gpsDoc["device_id"] = mqtt_client_id;
  
  // Add timestamp
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    gpsDoc["created_at"] = timeStr;
  } else {
    gpsDoc["timestamp"] = millis();
  }
  
  // Create nested location object
  JsonObject locationObj = gpsDoc.createNestedObject("location");
  locationObj["latitude"] = String(currentGpsData.latitude, 6);
  locationObj["longitude"] = String(currentGpsData.longitude, 6);
  
  if (currentGpsData.altitude != 0.0) {
    locationObj["altitude"] = currentGpsData.altitude;
  }
  
  if (currentGpsData.speed != 0.0) {
    locationObj["speed"] = currentGpsData.speed;
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(gpsDoc, mqttBuffer);
  
  // Publish GPS data
  bool published = mqttClient.publish(mqtt_topic_realtime, (const uint8_t*)mqttBuffer, n);
  
  if (published) {
    lastGpsUpdate = currentMillis;
    if (mqtt_debug) Serial.println("GPS data published successfully");
  } else if (mqtt_debug) {
    Serial.println("Failed to publish GPS data");
  }
  
  return published;
}

/**
 * Non-blocking fall detection data publisher
 */
bool publishFallDataNonBlocking() {
  unsigned long currentMillis = millis();
  
  // Only publish falls at most once per second
  if (currentMillis - lastFallUpdate < DATA_UPDATE_INTERVAL) {
    return false;
  }
  
  // Check if there is a fall event to publish
  if (!fallDetectionUpdated || !currentFallEvent.fallDetected) {
    return false;
  }
  
  // Create fall detection JSON document
  StaticJsonDocument<256> fallDoc;
  fallDoc["type"] = "fall";
  fallDoc["device_id"] = mqtt_client_id;
  fallDoc["fall_detected"] = 1; // We know it's detected at this point
  fallDoc["impact_strength"] = currentFallEvent.acceleration;
  
  // Add timestamp
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    fallDoc["created_at"] = timeStr;
  } else {
    fallDoc["timestamp"] = millis();
  }
  
  // Add detailed fall information
  JsonObject detailsObj = fallDoc.createNestedObject("details");
  
  // Map orientation to direction
  float pitch = currentFallEvent.orientation[0];
  String direction;
  if (pitch > 45) direction = "forward";
  else if (pitch < -45) direction = "backward";
  else direction = "sideways";
  detailsObj["direction"] = direction;
  
  detailsObj["severity"] = currentFallEvent.fallSeverity;
  
  // Add location if available
  if (gpsDataUpdated) {
    JsonObject locationObj = detailsObj.createNestedObject("location");
    locationObj["latitude"] = String(currentGpsData.latitude, 6);
    locationObj["longitude"] = String(currentGpsData.longitude, 6);
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(fallDoc, mqttBuffer);
  
  // Publish fall data
  bool published = mqttClient.publish(mqtt_topic_realtime, (const uint8_t*)mqttBuffer, n);
  
  if (published) {
    lastFallUpdate = currentMillis;
    if (mqtt_debug) Serial.println("Fall data published successfully");
  } else if (mqtt_debug) {
    Serial.println("Failed to publish fall data");
  }
  
  return published;
}

/**
 * Wrapper functions for backward compatibility with existing code
 * These functions call the non-blocking versions
 */

// Original publishEcgData function that other tasks call
void publishEcgData() {
  publishEcgDataNonBlocking();
}

// Original publishGpsData function that other tasks call
void publishGpsData() {
  publishGpsDataNonBlocking();
}

// Original publishFallData function that other tasks might call
void publishFallData() {
  publishFallDataNonBlocking();
}

// Original publishStatusUpdate function that other tasks might call
void publishStatusUpdate(bool forceUpdate) {
  publishStatusUpdateNonBlocking(forceUpdate);
}

/**
 * Main MQTT task with cooperative multitasking design
 */
void mqttTask(void *pvParameters) {
  // Initialize MQTT client
  setupMqtt();
  Serial.println("MQTT task started");
  
  // Simple cooperative task manager - no complex state machine needed
  unsigned long lastYieldTime = 0;
  
  // Initialize timing variables
  lastEcgUpdate = 0;
  lastGpsUpdate = 0;
  lastFallUpdate = 0;
  lastStatusUpdate = 0;
  lastConnectAttempt = 0;
  
  while (true) {
    // Always yield every 100ms maximum
    unsigned long currentTime = millis();
    if (currentTime - lastYieldTime >= 100) {
      vTaskDelay(pdMS_TO_TICKS(10)); // 10ms delay for yielding
      lastYieldTime = currentTime;
      continue; // Start fresh after yielding
    }
    
    // Step 1: Check WiFi connection (quick check)
    if (!getWiFiConnected()) {
      vTaskDelay(pdMS_TO_TICKS(500)); // Longer delay when no WiFi
      lastYieldTime = millis();
      continue;
    }
    
    // Step 2: Ensure MQTT connection (with non-blocking approach)
    if (!mqttClient.connected()) {
      bool connected = connectMqttNonBlocking();
      vTaskDelay(pdMS_TO_TICKS(10)); // Always yield after connection attempt
      lastYieldTime = millis();
      
      if (!connected) {
        vTaskDelay(pdMS_TO_TICKS(500)); // Longer delay if connection failed
        lastYieldTime = millis();
        continue;
      }
    }
    
    // Step 3: Process any pending MQTT messages (keep this quick)
    mqttClient.loop();
    vTaskDelay(pdMS_TO_TICKS(5)); // Short yield after MQTT loop
    
    // Step 4: Publish status update if needed
    if (publishStatusUpdateNonBlocking(false)) {
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield after publishing
      lastYieldTime = millis();
    }
    
    // Step 5: Publish ECG data (once per second max)
    if (publishEcgDataNonBlocking()) {
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield after publishing
      lastYieldTime = millis();
    }
    
    // Step 6: Publish GPS data (once per second max)
    if (publishGpsDataNonBlocking()) {
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield after publishing
      lastYieldTime = millis();
    }
    
    // Step 7: Publish fall detection data (if needed)
    if (publishFallDataNonBlocking()) {
      vTaskDelay(pdMS_TO_TICKS(10)); // Yield after publishing
      lastYieldTime = millis();
    }
    
    // Final step: Always end with a delay to avoid tight loops
    vTaskDelay(pdMS_TO_TICKS(50)); // Minimum 50ms between publishing cycles
    lastYieldTime = millis();
  }
}
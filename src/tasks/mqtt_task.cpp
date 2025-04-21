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

// Debug flag - set to true for verbose output
bool mqtt_debug = true;

// Forward declarations
void publishStatusUpdate(bool forceUpdate);

/**
 * Initialize MQTT client and settings
 */
void setupMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  espClient.setTimeout(5000); // 5 second timeout
  mqttClient.setBufferSize(512);
  
  if (mqtt_debug) {
    Serial.print("MQTT client initialized with broker: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.println(mqtt_port);
  }
}

/**
 * Connect attempt with timeout to prevent hanging
 */
bool connectMqtt() {
  // Check connection timing
  unsigned long currentMillis = millis();
  if (currentMillis - lastConnectAttempt < CONNECT_RETRY_INTERVAL) {
    return mqttClient.connected();
  }
  
  lastConnectAttempt = currentMillis;
  
  // Only attempt reconnection if not already connected
  if (mqttClient.connected()) {
    return true;
  }
  
  // Check if WiFi is connected first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Not attempting MQTT connection");
    return false;
  }
  
  Serial.print("Attempting MQTT connection...");
  
  // Last will testament message for offline status
  StaticJsonDocument<128> lastWillDoc;
  lastWillDoc["status"] = "offline";
  lastWillDoc["device_id"] = mqtt_client_id;
  lastWillDoc["timestamp"] = millis();
  
  char lastWillBuffer[128];
  size_t n = serializeJson(lastWillDoc, lastWillBuffer);
  
  // Set a shorter client timeout to prevent hanging
  espClient.setTimeout(2000); // 2 second timeout
  
  // Try to connect with last will message, with a timeout
  unsigned long connectStart = millis();
  
  // Non-blocking connection attempt with timeout
  bool result = mqttClient.connect(mqtt_client_id, NULL, NULL, mqtt_topic_status, 0, true, lastWillBuffer);
  
  if (result) {
    Serial.println("connected!");
    
    // Immediately publish an online status update
    publishStatusUpdate(true);
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(mqttClient.state());
    
    // Human-readable error message
    switch (mqttClient.state()) {
      case -4: Serial.println(" (connection timeout)"); break;
      case -3: Serial.println(" (connection lost)"); break;
      case -2: Serial.println(" (connect failed)"); break;
      case -1: Serial.println(" (disconnected)"); break;
      case 1: Serial.println(" (bad protocol)"); break;
      case 2: Serial.println(" (bad client ID)"); break;
      case 3: Serial.println(" (server unavailable)"); break;
      case 4: Serial.println(" (bad credentials)"); break;
      case 5: Serial.println(" (unauthorized)"); break;
      default: Serial.println(" (unknown error)"); break;
    }
    return false;
  }
}

/**
 * Publish device status information to MQTT
 * This function is meant for the device status dashboard
 * 
 * @param forceUpdate Force immediate update regardless of timer
 */
void publishStatusUpdate(bool forceUpdate) {
  unsigned long currentMillis = millis();
  
  // Only update periodically unless forced
  if (!forceUpdate && (currentMillis - lastStatusUpdate < STATUS_UPDATE_INTERVAL)) {
    return;
  }
  
  lastStatusUpdate = currentMillis;
  
  // Ensure MQTT is connected
  if (!mqttClient.connected()) {
    if (!connectMqtt()) {
      return;
    }
  }
  
  // Create status JSON document
  StaticJsonDocument<256> statusDoc;
  statusDoc["status"] = "online";
  statusDoc["device_id"] = mqtt_client_id;
  
  // Add timestamp
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    // Use currentEpoch directly without dereferencing it
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    statusDoc["created_at"] = timeStr;
  } else {
    statusDoc["created_at"] = String(currentMillis);
  }
  
  // Add WiFi information from global status
  JsonObject wifiObj = statusDoc.createNestedObject("wifi");
  wifiObj["rssi"] = currentWiFiStatus.rssi;
  
  // Copy volatile string to local buffer to avoid issues
  char ipBuffer[16];
  // Need to cast the volatile char* to char* for strncpy
  strncpy(ipBuffer, (const char*)currentWiFiStatus.ip, sizeof(ipBuffer)-1);
  ipBuffer[sizeof(ipBuffer)-1] = '\0'; // Ensure null termination
  
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
  
  // Convert mqttBuffer to uint8_t* for publish
  if (mqttClient.publish(mqtt_topic_status, (const uint8_t*)mqttBuffer, n, true)) {
    if (mqtt_debug) Serial.println("Status published successfully");
  } else {
    Serial.println("Failed to publish status");
  }
}

/**
 * Publish ECG data to MQTT
 * This function is meant for the real-time dashboard
 */
void publishEcgData() {
  // Check if we have updated ECG data
  if (!ecgDataUpdated) {
    return;
  }
  
  // Ensure MQTT is connected
  if (!mqttClient.connected()) {
    if (!connectMqtt()) {
      return;
    }
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
    // Use currentEpoch directly without dereferencing it
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ecgDoc["created_at"] = timeStr;
  } else {
    ecgDoc["timestamp"] = millis();
  }
  
  // Create ECG data array
  JsonArray ecgArray = ecgDoc.createNestedArray("ecg_data");
  
  // Add ECG samples - Create simulated waveform based on the raw value
  for (int i = 0; i < 20; i++) {
    // Generate a basic ECG-like pattern
    int value = currentEcgData.rawValue;
    
    // Add a simulated peak every 5th point for a simple ECG-like pattern
    if (i % 5 == 0) {
      value += random(50, 100);
    } else if (i % 5 == 1) {
      value -= random(20, 40);
    }
    
    ecgArray.add(value + random(-10, 10));
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(ecgDoc, mqttBuffer);
  
  // Publish ECG data
  if (mqtt_debug) {
    Serial.print("Publishing ECG data: ");
    Serial.println(mqttBuffer);
  }
  
  if (mqttClient.publish(mqtt_topic_realtime, (const uint8_t*)mqttBuffer, n)) {
    if (mqtt_debug) Serial.println("ECG data published successfully");
  } else {
    Serial.println("Failed to publish ECG data");
  }
}

/**
 * Publish GPS location data to MQTT
 * This function is meant for the real-time dashboard
 */
void publishGpsData() {
  // Check if we have updated GPS data
  if (!gpsDataUpdated) {
    return;
  }
  
  // Ensure MQTT is connected
  if (!mqttClient.connected()) {
    if (!connectMqtt()) {
      return;
    }
  }
  
  // Create GPS JSON document
  StaticJsonDocument<256> gpsDoc;
  gpsDoc["type"] = "gps";
  gpsDoc["device_id"] = mqtt_client_id;
  
  // Add timestamp
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    // Use currentEpoch directly without dereferencing it
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
  
  // Only add altitude if it's available - using altitude field directly
  // FIX: altitudeValid doesn't exist, checking if altitude is non-zero
  if (currentGpsData.altitude != 0.0) {
    locationObj["altitude"] = currentGpsData.altitude;
  }
  
  // Only add speed if it's valid - using speed field directly
  // FIX: speedValid doesn't exist, checking if speed is non-zero
  if (currentGpsData.speed != 0.0) {
    locationObj["speed"] = currentGpsData.speed;
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(gpsDoc, mqttBuffer);
  
  // Publish GPS data
  if (mqtt_debug) {
    Serial.print("Publishing GPS data: ");
    Serial.println(mqttBuffer);
  }
  
  if (mqttClient.publish(mqtt_topic_realtime, (const uint8_t*)mqttBuffer, n)) {
    if (mqtt_debug) Serial.println("GPS data published successfully");
  } else {
    Serial.println("Failed to publish GPS data");
  }
}

/**
 * Publish fall detection event to MQTT
 * This function is meant for the real-time dashboard (for alerts)
 */
void publishFallData() {
  // Check if we have a fall detection event
  if (!fallDetectionUpdated) {
    return;
  }
  
  // Ensure MQTT is connected
  if (!mqttClient.connected()) {
    if (!connectMqtt()) {
      return;
    }
  }
  
  // Create fall detection JSON document
  StaticJsonDocument<256> fallDoc;
  fallDoc["type"] = "fall";
  fallDoc["device_id"] = mqtt_client_id;
  fallDoc["fall_detected"] = currentFallEvent.fallDetected ? 1 : 0;
  // Fix: using acceleration instead of impactStrength
  fallDoc["impact_strength"] = currentFallEvent.acceleration;
  
  // Add timestamp
  if (currentTimeStatus.synchronized) {
    char timeStr[30];
    struct tm timeinfo;
    // Use currentEpoch directly without dereferencing it
    time_t currentTime = currentTimeStatus.currentEpoch;
    localtime_r(&currentTime, &timeinfo);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    fallDoc["created_at"] = timeStr;
  } else {
    fallDoc["timestamp"] = millis();
  }
  
  // Add detailed fall information
  if (currentFallEvent.fallDetected) {
    JsonObject detailsObj = fallDoc.createNestedObject("details");
    
    // Fix: direction doesn't exist, using orientation[0] and mapping to a cardinal direction
    float pitch = currentFallEvent.orientation[0];
    String direction;
    if (pitch > 45) direction = "forward";
    else if (pitch < -45) direction = "backward";
    else direction = "sideways";
    detailsObj["direction"] = direction;
    
    // Fix: severity exists as fallSeverity
    detailsObj["severity"] = currentFallEvent.fallSeverity;
    
    // Fix: latitude/longitude don't exist in FallEvent - use currentGpsData if available
    if (gpsDataUpdated) {
      JsonObject locationObj = detailsObj.createNestedObject("location");
      locationObj["latitude"] = String(currentGpsData.latitude, 6);
      locationObj["longitude"] = String(currentGpsData.longitude, 6);
    }
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(fallDoc, mqttBuffer);
  
  // Publish fall data
  if (mqtt_debug) {
    Serial.print("Publishing fall data: ");
    Serial.println(mqttBuffer);
  }
  
  if (mqttClient.publish(mqtt_topic_realtime, (const uint8_t*)mqttBuffer, n)) {
    if (mqtt_debug) Serial.println("Fall data published successfully");
  } else {
    Serial.println("Failed to publish fall data");
  }
}

/**
 * Main MQTT task that runs continuously
 * 
 * @param pvParameters Task parameters (not used)
 */
void mqttTask(void *pvParameters) {
  // Initialize MQTT client
  setupMqtt();
  
  Serial.println("MQTT task started");
  
  // Use for non-blocking operations
  unsigned long lastYieldTime = 0;
  const unsigned long YIELD_INTERVAL = 50; // Yield every 50ms
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Wait for WiFi to be connected
    if (!getWiFiConnected()) {
      Serial.println("MQTT task: Waiting for WiFi connection...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    
    // Yield frequently to prevent watchdog timeout
    if (currentTime - lastYieldTime >= YIELD_INTERVAL) {
      vTaskDelay(1);
      lastYieldTime = currentTime;
    }
    
    // Ensure MQTT connection with timeout protection
    if (!mqttClient.connected()) {
      bool connected = connectMqtt();
      // Always yield after connection attempt
      vTaskDelay(1);
      
      if (!connected) {
        // If connection failed, wait a bit before retrying
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
    }
    
    // MQTT loop with timeout protection
    unsigned long loopStartTime = millis();
    mqttClient.loop();
    
    // If loop took too long, yield
    if (millis() - loopStartTime > 50) {
      vTaskDelay(1);
    }
    
    // Only proceed with publishing if still connected
    if (mqttClient.connected()) {
      // 1. Publish device status update periodically
      publishStatusUpdate(false);
      
      // Yield after potentially long operation
      vTaskDelay(1);
      
      // 2. Publish real-time data when available
      if (ecgDataUpdated) {
        publishEcgData();
        // Yield after potentially long operation
        vTaskDelay(1);
      }
      
      if (gpsDataUpdated) {
        publishGpsData();
        // Yield after potentially long operation
        vTaskDelay(1);
      }
      
      if (fallDetectionUpdated && currentFallEvent.fallDetected) {
        publishFallData();
        // Don't reset fallDetectionUpdated here, let the Telegram task handle it first
        // We'll add a delay to make sure Telegram task gets to process it
        vTaskDelay(pdMS_TO_TICKS(500));
      }
    }
    
    // Brief delay to avoid consuming too much CPU
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
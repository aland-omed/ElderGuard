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

// MQTT Settings
const char* mqtt_server = "broker.hivemq.com"; // Alternative free public broker
const int mqtt_port = 1883;
const char* mqtt_client_id = "ElderGuard_Device";
const char* mqtt_topic_ecg = "elderguard/patient/1/realtime";  // More specific topic
const char* mqtt_topic_location = "elderguard/patient/1/realtime"; // Using same topic for simplicity

// MQTT Client
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQTT message buffer
char mqttBuffer[512];

void setupMqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  Serial.println("MQTT client initialized");
}

void reconnectMqtt() {
  // Loop until we're reconnected
  int retries = 0;
  while (!mqttClient.connected() && retries < 5) {
    Serial.print("Attempting MQTT connection...");
    
    // Debug DNS resolution
    IPAddress resolvedIP;
    if (!WiFi.hostByName(mqtt_server, resolvedIP)) {
      Serial.print("[ERROR] DNS lookup failed for ");
      Serial.println(mqtt_server);
      Serial.println("Checking WiFi status...");
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Waiting for reconnection...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        continue;
      }
    } else {
      Serial.print("Resolved IP for broker: ");
      Serial.println(resolvedIP.toString());
    }
    
    // Attempt to connect with last will testament
    if (mqttClient.connect(mqtt_client_id, NULL, NULL, "elderguard/status", 0, true, "offline")) {
      Serial.println("connected");
      // Publish online status
      mqttClient.publish("elderguard/status", "online", true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      retries++;
      // Wait before retrying
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
}

void publishEcgData(int rawValue, int heartRate, bool validSignal) {
  // Only publish if connected to MQTT
  if (!mqttClient.connected()) {
    reconnectMqtt();
    if (!mqttClient.connected()) {
      Serial.println("Failed to reconnect to MQTT, not publishing ECG data");
      return;
    }
  }

  // Create JSON document for ECG data
  StaticJsonDocument<256> jsonDoc;
  
  // Add ECG data
  jsonDoc["heart_rate"] = heartRate;
  jsonDoc["valid_signal"] = validSignal ? 1 : 0;
  
  // Create an array for ECG waveform (simplified)
  JsonArray ecgData = jsonDoc.createNestedArray("ecg_data");
  for (int i = 0; i < 50; i++) {
    // Here you could use actual ECG data points from a buffer
    // For now, we're just adding the raw value with some variation
    ecgData.add(rawValue + random(-50, 50));
  }
  
  // Serialize JSON to buffer
  size_t n = serializeJson(jsonDoc, mqttBuffer);
  
  // Publish message
  Serial.print("Publishing ECG data: ");
  Serial.println(mqttBuffer);
  mqttClient.publish(mqtt_topic_ecg, mqttBuffer, n);
}

void publishGpsData(float latitude, float longitude) {
  // Only publish if connected to MQTT
  if (!mqttClient.connected()) {
    reconnectMqtt();
    if (!mqttClient.connected()) {
      Serial.println("Failed to reconnect to MQTT, not publishing GPS data");
      return;
    }
  }

  // Create JSON document for location data
  StaticJsonDocument<256> jsonDoc;
  
  // Nest the location data in a location object
  JsonObject locationObj = jsonDoc.createNestedObject("location");
  locationObj["latitude"] = String(latitude, 6);  // Convert to string with 6 decimal places
  locationObj["longitude"] = String(longitude, 6);
  
  // Serialize JSON to buffer
  size_t n = serializeJson(jsonDoc, mqttBuffer);
  
  // Publish message
  Serial.print("Publishing GPS data: ");
  Serial.println(mqttBuffer);
  mqttClient.publish(mqtt_topic_location, mqttBuffer, n);
}

void mqttTask(void *pvParameters) {
  // Initialize MQTT
  setupMqtt();
  
  // Wait for WiFi to be connected
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    Serial.println("MQTT task waiting for WiFi connection...");
  }
  
  Serial.println("MQTT task starting main loop");
  
  while (1) {
    // Ensure MQTT is connected
    if (!mqttClient.connected()) {
      reconnectMqtt();
    }
    
    // Handle MQTT loop
    mqttClient.loop();
    
    // Yield to other tasks
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
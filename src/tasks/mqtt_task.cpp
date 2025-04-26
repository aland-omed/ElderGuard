/**
 * mqtt_task.cpp
 *
 * ElderGuard - MQTT Task Implementation
 * Sends only ECG and GPS data (and periodic status) over TLS
 * Uses HiveMQ Cloud Serverless on port 8883, FreeRTOS-friendly, non-blocking.
 */

#include "../include/mqtt_task.h"
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/wifi_task.h"
#include "../include/ecg_task.h"
#include "../include/gps_task.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// HiveMQ Cloud Serverless connection details
static constexpr char MQTT_SERVER[]    = "b7566807e82d4efc999f9b5d375936bf.s1.eu.hivemq.cloud";
static constexpr uint16_t MQTT_PORT    = 8883;            // TLS MQTT port
static constexpr char MQTT_CLIENT_ID[] = "ElderGuard_Device";
static constexpr char MQTT_USER[]      = "aland_omed";
static constexpr char MQTT_PASS[]      = "Aland123";

// MQTT topics
static constexpr char TOPIC_REALTIME[] = "elderguard/patient/1/realtime";
static constexpr char TOPIC_STATUS[]   = "elderguard/patient/1/status";

// Underlying TLS client and PubSubClient
static WiFiClientSecure tlsClient;
static PubSubClient mqttClient(tlsClient);

// Status publish interval
static const unsigned long STATUS_INTERVAL = 30000UL;
static unsigned long lastStatusTs = 0;
static unsigned long lastConnectAttempt = 0;
static const unsigned long CONNECT_RETRY_INTERVAL = 5000UL; // Only try to connect every 5 seconds

/**
 * Initialize the MQTT client
 */
void setupMqtt() {
    tlsClient.setInsecure();
    tlsClient.setTimeout(1); // Set very short timeout to prevent blocking
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setKeepAlive(15); // 15 seconds keepalive
    mqttClient.setSocketTimeout(1); // 1 second socket timeout to avoid blocking
    // No incoming callbacks required
}

/**
 * Attempt non-blocking MQTT connect with timeout check
 */
bool connectMqttNonBlocking() {
    if (mqttClient.connected()) return true;
    
    // Only attempt to connect once every CONNECT_RETRY_INTERVAL
    unsigned long now = millis();
    if (now - lastConnectAttempt < CONNECT_RETRY_INTERVAL) {
        return false;
    }
    
    lastConnectAttempt = now;
    
    // Set a connect timeout
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, TOPIC_STATUS, 0, true, "{\"status\":\"offline\"}", true)) {
        // Publish retained "online" status
        StaticJsonDocument<128> doc;
        doc["status"] = "online";
        char buf[128];
        size_t n = serializeJson(doc, buf);
        mqttClient.publish(TOPIC_STATUS, (uint8_t*)buf, n, true);
        return true;
    } else {
        return false;
    }
}

/**
 * Publish ECG data to MQTT broker - with connection and timeout checks
 */
void publishEcgData() {
    if (!mqttClient.connected() || !ecgDataUpdated) {
        return;
    }
    
    // Take a local copy of the current ECG data to avoid race conditions
    int localHeartRate = currentEcgData.heartRate;
    bool localValidSignal = currentEcgData.validSignal;
    
    // Create a static document to prevent memory fragmentation
    static StaticJsonDocument<512> doc;
    doc.clear();
    doc["type"] = "ecg";
    doc["heart_rate"] = localHeartRate;
    doc["valid"] = localValidSignal ? 1 : 0;
    doc["timestamp"] = time(nullptr);
    
    // Add raw ECG data samples from the circular buffer
    JsonArray samples = doc.createNestedArray("ecg_data");
    
    // Add a limited number of samples to avoid excessive data
    const int numSamples = 10; // Reduced from 30 to minimize packet size
    
    // Copy sample data to a local buffer before adding to JSON to avoid race conditions
    int localSamples[numSamples];
    // Safely obtain ECG buffer data
    if (xSemaphoreTake(ecgDataSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (int i = 0; i < numSamples; i++) {
            int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
            localSamples[i] = ecgBuffer[idx];
        }
        xSemaphoreGive(ecgDataSemaphore);
    } else {
        // If we can't get the semaphore, just use zeros
        for (int i = 0; i < numSamples; i++) {
            localSamples[i] = 0;
        }
    }
    
    // Now add the local samples to the JSON document
    for (int i = 0; i < numSamples; i++) {
        samples.add(localSamples[i]);
    }
    
    // Serialize the data
    char buf[512];
    size_t n = serializeJson(doc, buf);
    
    // Publish
    bool published = mqttClient.publish(TOPIC_REALTIME, (uint8_t*)buf, n);
    if (published) {
        ecgDataUpdated = false;
    }
}

/**
 * Publish GPS data to MQTT broker - with connection and timeout check
 */
void publishGpsData() {
    if (!mqttClient.connected() || !gpsDataUpdated) {
        return;
    }
    
    // Take a local copy of GPS data to avoid race conditions
    float localLat = 0, localLng = 0;
    bool validGpsData = false;
    
    if (xSemaphoreTake(gpsDataSemaphore, pdMS_TO_TICKS(10)) == pdTRUE) {
        localLat = currentGpsData.latitude;
        localLng = currentGpsData.longitude;
        validGpsData = currentGpsData.validFix;
        xSemaphoreGive(gpsDataSemaphore);
    } else {
        // If we can't get the semaphore, skip publication
        return;
    }
    
    // Only publish if we have valid GPS data
    if (validGpsData) {
        StaticJsonDocument<256> doc;
        doc["type"]      = "gps";
        doc["lat"]       = localLat;
        doc["lng"]       = localLng;
        doc["timestamp"] = time(nullptr);
        char buf[256];
        size_t n = serializeJson(doc, buf);
        bool published = mqttClient.publish(TOPIC_REALTIME, (uint8_t*)buf, n);
        if (published) {
            gpsDataUpdated = false;
        }
    }
}

/**
 * Main FreeRTOS MQTT task - with yield guarantees
 */
void mqttTask(void* pvParameters) {
    setupMqtt();
    
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(20);

    while (true) {
        // Ensure we yield regularly using vTaskDelayUntil instead of vTaskDelay
        // This ensures consistent timing and prevents watchdog issues
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Skip all MQTT operations if WiFi is not connected
        if (!getWiFiConnected()) {
            continue;
        }

        // Try to connect, but with timeout protection
        connectMqttNonBlocking();
        
        // Only call loop() if we're connected to avoid blocking
        if (mqttClient.connected()) {
            mqttClient.loop();
            
            // Publish data if needed - these functions have their own connection checks
            publishEcgData();
            publishGpsData();
            
            // Send periodic status updates
            unsigned long now = millis();
            if (now - lastStatusTs >= STATUS_INTERVAL) {
                lastStatusTs = now;
                
                StaticJsonDocument<128> doc;
                doc["status"] = "online";
                char buf[128];
                size_t n = serializeJson(doc, buf);
                mqttClient.publish(TOPIC_STATUS, (uint8_t*)buf, n, true);
            }
        }
        
        // Yield to other tasks
        taskYIELD();
    }
}

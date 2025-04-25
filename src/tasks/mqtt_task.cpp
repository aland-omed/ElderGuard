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

/**
 * Initialize the MQTT client
 */
void setupMqtt() {
    tlsClient.setInsecure();
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    // No incoming callbacks required
}

/**
 * Attempt non-blocking MQTT connect
 */
bool connectMqttNonBlocking() {
    if (mqttClient.connected()) return true;
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
        // Publish retained "online" status
        StaticJsonDocument<128> doc;
        doc["status"] = "online";
        char buf[128];
        size_t n = serializeJson(doc, buf);
        mqttClient.publish(TOPIC_STATUS, (uint8_t*)buf, n, true);
        return true;
    }
    return false;
}

/**
 * Publish ECG data to MQTT broker
 * Enhanced to include raw ECG samples
 */
void publishEcgData() {
    if (ecgDataUpdated) {
        StaticJsonDocument<512> doc;
        doc["type"] = "ecg";
        doc["heart_rate"] = currentEcgData.heartRate;
        doc["valid"] = currentEcgData.validSignal ? 1 : 0;
        doc["timestamp"] = time(nullptr);
        
        // Add raw ECG data samples from the circular buffer
        JsonArray samples = doc.createNestedArray("ecg_data");
        
        // Add the most recent 30 samples for visualization
        const int numSamples = 30;
        for (int i = 0; i < numSamples; i++) {
            int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
            samples.add(ecgBuffer[idx]);
        }
        
        char buf[512];
        size_t n = serializeJson(doc, buf);
        
        // Check if MQTT client is connected before publishing
        if (mqttClient.connected()) {
            mqttClient.publish(TOPIC_REALTIME, (uint8_t*)buf, n);
        }
        ecgDataUpdated = false;
    }
}

/**
 * Publish GPS data to MQTT broker
 */
void publishGpsData() {
    if (gpsDataUpdated) {
        StaticJsonDocument<256> doc;
        doc["type"]      = "gps";
        doc["lat"]       = currentGpsData.latitude;
        doc["lng"]       = currentGpsData.longitude;
        doc["timestamp"] = time(nullptr);
        char buf[256];
        size_t n = serializeJson(doc, buf);
        mqttClient.publish(TOPIC_REALTIME, (uint8_t*)buf, n);
        gpsDataUpdated = false;
    }
}

/**
 * Main FreeRTOS MQTT task
 */
void mqttTask(void* pvParameters) {
    setupMqtt();

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(20));

        if (!getWiFiConnected()) {
            continue;
        }

        connectMqttNonBlocking();
        mqttClient.loop();

        // Re-enable ECG data publishing, now with raw data
        publishEcgData();
        publishGpsData();

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
}

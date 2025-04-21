/**
 * ElderGuard - MQTT Task Header
 * 
 * Defines the MQTT task and related functions for sending telemetry
 * data to the cloud.
 */

#ifndef MQTT_TASK_H
#define MQTT_TASK_H

#include <Arduino.h>

// Function prototypes
void mqttTask(void *pvParameters);
void setupMqtt();
bool connectMqtt();
void publishStatusUpdate(bool forceUpdate);
void publishEcgData();
void publishGpsData();
void publishFallData();

#endif // MQTT_TASK_H
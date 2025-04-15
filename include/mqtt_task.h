/**
 * ElderGuard - MQTT Task
 * 
 * Handles MQTT connectivity and data publishing for the ElderGuard system
 */

#ifndef MQTT_TASK_H
#define MQTT_TASK_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>

// Function declarations
void mqttTask(void *pvParameters);
void publishEcgData(int rawValue, int heartRate, bool validSignal);
void publishGpsData(float latitude, float longitude);
void setupMqtt();
void reconnectMqtt();

#endif // MQTT_TASK_H
/**
 * ElderGuard - HTTP Task
 * 
 * This file contains the declarations for simplified HTTP data task
 * that sends essential sensor data (heart rate, ECG, location) to the Laravel backend.
 */

#ifndef HTTP_TASK_H
#define HTTP_TASK_H

#include <Arduino.h>
#include "config.h"
#include "globals.h"

// Main HTTP task function
void httpTask(void *pvParameters);

// Data sending functions
void sendSensorData();
void sendHeartRateAlert(int heartRate);
void sendLocationData();
void sendTelegramMessage(const char* message);  // Added declaration for Telegram function

#endif // HTTP_TASK_H
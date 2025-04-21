/**
 * ElderGuard - Firmware Update Task
 * 
 * This file contains the declarations for OTA firmware updates
 * that connect to the Laravel backend to check for and download updates.
 */

#ifndef FIRMWARE_UPDATE_TASK_H
#define FIRMWARE_UPDATE_TASK_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Current firmware version - update this when you release new versions
#define FIRMWARE_VERSION "1.2.3"

// URLs must match exactly with Laravel routes - removed the leading slash
#define FIRMWARE_API_URL "https://elderguard.codecommerce.info/elderguard/firmware.bin"
#define FIRMWARE_REPORT_URL "https://elderguard.codecommerce.info/elderguard/report-update"

// Update check interval (in milliseconds) - every 24 hours
#define FIRMWARE_UPDATE_CHECK_INTERVAL 86400000

// Main firmware update task
void firmwareUpdateTask(void *pvParameters);

// Check for firmware updates
bool checkFirmwareUpdate();

// Report update status to the server
void reportUpdateStatus(const char* version, const char* status);

#endif // FIRMWARE_UPDATE_TASK_H
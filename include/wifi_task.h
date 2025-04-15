/**
 * ElderGuard - WiFi Management Task Header
 * 
 * This file contains declarations for WiFi connectivity management in the ElderGuard system.
 */

#ifndef WIFI_TASK_H
#define WIFI_TASK_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"

/**
 * Main WiFi management task function
 * This task handles WiFi connection setup, monitoring, and reconnection
 * 
 * @param pvParameters Task parameters (not used)
 */
void wifiTask(void *pvParameters);

/**
 * Initialize WiFi configuration
 * 
 * @param status Pointer to WiFi status structure
 */
void setupWiFi(volatile WiFiStatus *status);

/**
 * Attempt to connect to WiFi network
 * 
 * @param status Pointer to WiFi status structure
 * @return true if connection successful, false otherwise
 */
bool connectToWiFi(volatile WiFiStatus *status);

/**
 * Attempt to reconnect to WiFi if disconnected
 * 
 * @param status Pointer to WiFi status structure
 * @return true if reconnection successful, false otherwise
 */
bool reconnectWiFi(volatile WiFiStatus *status);

/**
 * Update WiFi status information (RSSI, IP, etc.)
 * 
 * @param status Pointer to WiFi status structure
 */
void updateWiFiStatus(volatile WiFiStatus *status);

/**
 * Get current WiFi connection status
 * 
 * @return true if connected, false otherwise
 */
bool getWiFiConnected();

#endif // WIFI_TASK_H
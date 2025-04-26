/**
 * ElderGuard - WiFi Management Task Implementation
 * 
 * This file implements the WiFi connectivity functionality that handles
 * connecting to WiFi networks and monitoring connection status.
 */

#include <Arduino.h>
#include <WiFi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "../include/wifi_task.h"
#include "../include/config.h"
#include "../include/globals.h"

void wifiTask(void *pvParameters) {
  // Initialize WiFi status
  currentWiFiStatus.connected = false;
  currentWiFiStatus.rssi = 0;
  
  // Use a temporary non-volatile buffer for string operations
  char buffer[16];
  strcpy(buffer, "0.0.0.0");
  memcpy((void*)currentWiFiStatus.ip, buffer, strlen(buffer) + 1);
  
  currentWiFiStatus.lastConnectAttempt = 0;
  currentWiFiStatus.failureCount = 0;
  currentWiFiStatus.lastStatusCheck = 0;
  
  // Setup WiFi configuration
  setupWiFi(&currentWiFiStatus);
  
  // Initial connection attempt
  connectToWiFi(&currentWiFiStatus);
  
  // Main task loop
  while (true) {
    // Check if WiFi is connected
    if (!currentWiFiStatus.connected) {
      // Attempt to reconnect if it's been long enough since the last attempt
      if (millis() - currentWiFiStatus.lastConnectAttempt > WIFI_RECONNECT_INTERVAL_MS) {
        reconnectWiFi(&currentWiFiStatus);
      }
    } else {
      // Update WiFi status information (RSSI, IP, etc.)
      updateWiFiStatus(&currentWiFiStatus);
    }
    
    // Signal other tasks that WiFi status is updated
    xSemaphoreGive(wifiStatusSemaphore);
    
    // Delay before next check
    vTaskDelay(pdMS_TO_TICKS(WIFI_TASK_INTERVAL_MS));
  }
}

void setupWiFi(volatile WiFiStatus *status) {
  // Set WiFi mode to station (client)
  WiFi.mode(WIFI_STA);
  
  // Disconnect from any previous connections
  WiFi.disconnect();
  
  // Set hostname for easier identification on network
  WiFi.setHostname("ElderGuard");
  
  // Wait a moment for WiFi to initialize
  delay(100);
}

bool connectToWiFi(volatile WiFiStatus *status) {
  // Record connection attempt time
  status->lastConnectAttempt = millis();
  
  // Begin connection attempt
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait for connection or timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
  }
  
  // Check if connected
  if (WiFi.status() == WL_CONNECTED) {
    // Update status information
    status->connected = true;
    status->rssi = WiFi.RSSI();
    
    // Use a temporary buffer for string operations
    char buffer[16];
    strcpy(buffer, WiFi.localIP().toString().c_str());
    memcpy((void*)status->ip, buffer, strlen(buffer) + 1);
    
    status->failureCount = 0;
    
    // Update shared WiFi status
    xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100));
    wifiStatusUpdated = true;
    xSemaphoreGive(displayMutex);
    
    return true;
  } else {
    // Update failure information
    status->connected = false;
    status->failureCount++;
    
    // Update shared WiFi status
    xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100));
    wifiStatusUpdated = true;
    xSemaphoreGive(displayMutex);
    
    return false;
  }
}

bool reconnectWiFi(volatile WiFiStatus *status) {
  // Just use the connect function for reconnection
  return connectToWiFi(status);
}

void updateWiFiStatus(volatile WiFiStatus *status) {
  // Only update status periodically to avoid excessive overhead
  if (millis() - status->lastStatusCheck < WIFI_TASK_INTERVAL_MS) {
    return;
  }
  
  status->lastStatusCheck = millis();
  
  // Check current connection status
  if (WiFi.status() != WL_CONNECTED) {
    // If we thought we were connected but we're not anymore
    if (status->connected) {
      status->connected = false;
      
      // Update shared WiFi status
      xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100));
      wifiStatusUpdated = true;
      xSemaphoreGive(displayMutex);
    }
    return;
  }
  
  // Update RSSI and IP address
  int newRssi = WiFi.RSSI();
  String newIp = WiFi.localIP().toString();
  
  // Copy volatile data to temporary buffer for comparison
  char currentIp[16];
  memcpy(currentIp, (const void*)status->ip, sizeof(currentIp));
  
  // Only update if there's been a significant change in RSSI or IP
  if (abs(status->rssi - newRssi) > 5 || strcmp(currentIp, newIp.c_str()) != 0) {
    status->rssi = newRssi;
    
    // Use a temporary buffer for string operations
    char buffer[16];
    strcpy(buffer, newIp.c_str());
    memcpy((void*)status->ip, buffer, strlen(buffer) + 1);
    
    // Update shared WiFi status with mutex protection
    xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100));
    wifiStatusUpdated = true;
    xSemaphoreGive(displayMutex);
  }
}

bool getWiFiConnected() {
  return currentWiFiStatus.connected;
}
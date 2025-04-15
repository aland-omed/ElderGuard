/**
 * ElderGuard - GPS Task Implementation
 * 
 * This file implements the GPS tracking task using the GY-NEO6MV2 module.
 * It retrieves longitude and latitude and sends data through MQTT every second
 * and through HTTP every 40 seconds.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <TinyGPS++.h>
#include "../include/gps_task.h"
#include "../include/config.h"

// External queue handle declared in main.cpp
extern QueueHandle_t gpsDataQueue;

// TinyGPS++ object
TinyGPSPlus gps;

// Variables for MQTT and HTTP publishing
unsigned long lastMqttPublish = 0;
unsigned long lastHttpPublish = 0;

// GPS data structure
GpsData currentGpsData;

void gpsTask(void *pvParameters) {
  Serial.println("GPS Task: Started");
  
  // Main task loop
  while (true) {
    // Process GPS data while available
    while (Serial2.available() > 0) {
      gps.encode(Serial2.read());
    }
    
    // Process GPS data at regular intervals
    unsigned long currentTime = millis();
    
    // Update GPS data
    updateGpsData(currentTime);
    
    // MQTT publishing (every second)
    if (currentTime - lastMqttPublish >= MQTT_PUBLISH_INTERVAL_MS) {
      lastMqttPublish = currentTime;
      
      // Send to queue for MQTT publishing
      xQueueSend(gpsDataQueue, &currentGpsData, 0);
      
      // Debug output
      printGpsDebugInfo();
    }
    
    // HTTP publishing (every 40 seconds)
    if (currentTime - lastHttpPublish >= HTTP_PUBLISH_INTERVAL_MS) {
      lastHttpPublish = currentTime;
      
      // For now, just print that we would send HTTP data
      Serial.println("GPS Task: Would send HTTP data now");
      // This would be replaced with actual HTTP code later
    }
    
    // Small delay to prevent task from hogging CPU
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void updateGpsData(unsigned long timestamp) {
  // Update GPS data structure with latest values
  currentGpsData.timestamp = timestamp;
  
  // Only update location if we have a valid GPS fix
  if (gps.location.isValid()) {
    currentGpsData.latitude = gps.location.lat();
    currentGpsData.longitude = gps.location.lng();
    currentGpsData.validFix = true;
    
    // Update additional data if available
    if (gps.altitude.isValid()) {
      currentGpsData.altitude = gps.altitude.meters();
    }
    
    if (gps.speed.isValid()) {
      currentGpsData.speed = gps.speed.kmph();
    }
    
    if (gps.satellites.isValid()) {
      currentGpsData.satellites = gps.satellites.value();
    }
  } else {
    currentGpsData.validFix = false;
  }
}

void printGpsDebugInfo() {
  // Print debug information about GPS status
  Serial.println("\n--- GPS DATA ---");
  
  // Satellite information
  Serial.print("Satellites: ");
  if (gps.satellites.isValid()) {
    Serial.println(gps.satellites.value());
  } else {
    Serial.println("Unknown");
  }
  
  // Location information
  Serial.print("Location: ");
  if (currentGpsData.validFix) {
    Serial.print(currentGpsData.latitude, 6);
    Serial.print(", ");
    Serial.println(currentGpsData.longitude, 6);
    
    Serial.print("Altitude: ");
    Serial.print(currentGpsData.altitude);
    Serial.println(" meters");
    
    Serial.print("Speed: ");
    Serial.print(currentGpsData.speed);
    Serial.println(" km/h");
  } else {
    Serial.println("No valid fix");
  }
  
  // HDOP (Horizontal Dilution of Precision)
  Serial.print("HDOP (Precision): ");
  if (gps.hdop.isValid()) {
    Serial.print(gps.hdop.value()/100.0, 2);
    Serial.println(" (lower is better)");
  } else {
    Serial.println("Unknown");
  }
  
  Serial.println("-----------------");
}
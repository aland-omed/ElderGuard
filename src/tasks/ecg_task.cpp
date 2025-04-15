/**
 * ElderGuard - ECG & Heart Rate Task Implementation
 * 
 * This file implements the ECG monitoring task using the AD8232 module.
 * It samples ECG data, calculates heart rate, and sends data to the server
 * through the MQTT protocol every second.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "../include/ecg_task.h"
#include "../include/config.h"

// External queue handle declared in main.cpp
extern QueueHandle_t ecgDataQueue;

// Constants for ECG processing
#define ECG_BUFFER_SIZE 250              // 5 seconds at 50Hz
#define PEAK_THRESHOLD 3650              // Threshold for R-peak detection
#define RESET_THRESHOLD 3550             // Threshold to reset peak detection
#define MIN_RR_MS 300                    // Minimum time between R-peaks (200 BPM max)
#define MAX_RR_MS 1200                   // Maximum time between R-peaks (50 BPM min)
#define SAMPLE_INTERVAL_MS (1000 / ECG_SAMPLE_FREQUENCY_HZ) // Time between samples

// Circular buffer for ECG samples
int ecgBuffer[ECG_BUFFER_SIZE];
int bufferIndex = 0;

// Variables for heart rate calculation
bool isPeak = false;
unsigned long lastPeakTime = 0;
int heartRate = 0;

void ecgTask(void *pvParameters) {
  // Setup ADC for ECG input
  adc1_config_width(ADC_WIDTH_BIT_12);                   // 12-bit resolution (0-4095)
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12); // Changed from ADC_ATTEN_DB_11 to ADC_ATTEN_DB_12
  
  // Configure lead-off detection pins if used
  pinMode(ECG_LO_POS_PIN, INPUT);
  pinMode(ECG_LO_NEG_PIN, INPUT);
  
  Serial.println("ECG Task: Started");

  // Variables for task timing
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLE_INTERVAL_MS);
  xLastWakeTime = xTaskGetTickCount();
  
  // Variables for MQTT publishing
  unsigned long lastMqttPublish = 0;
  
  // Main task loop
  while (true) {
    // Ensure task runs at consistent frequency
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    // Read ECG value
    int ecgValue = adc1_get_raw(ADC1_CHANNEL_0);
    
    // Store in circular buffer
    ecgBuffer[bufferIndex] = ecgValue;
    bufferIndex = (bufferIndex + 1) % ECG_BUFFER_SIZE;
    
    // Check for valid signal using lead-off detection if available
    bool validSignal = isValidEcgSignal();
    
    // R-peak detection for heart rate calculation
    if (ecgValue > PEAK_THRESHOLD && !isPeak && validSignal) {
      isPeak = true;
      unsigned long currentTime = millis();
      
      if (lastPeakTime > 0) {
        unsigned long rr = currentTime - lastPeakTime;
        
        // Validate RR interval is physiologically reasonable
        if (rr >= MIN_RR_MS && rr <= MAX_RR_MS) {
          heartRate = 60000 / rr; // Convert RR interval to BPM
        }
      }
      
      lastPeakTime = currentTime;
    }
    
    // Reset peak detection when signal drops below threshold
    if (ecgValue < RESET_THRESHOLD) {
      isPeak = false;
    }
    
    // Prepare data for queues and MQTT publishing
    unsigned long currentTime = millis();
    if (currentTime - lastMqttPublish >= MQTT_PUBLISH_INTERVAL_MS) {
      lastMqttPublish = currentTime;
      
      // Create ECG data structure
      EcgData ecgData;
      ecgData.rawValue = ecgValue;
      ecgData.heartRate = heartRate;
      ecgData.validSignal = validSignal;
      ecgData.timestamp = currentTime;
      
      // Send to queue for other tasks (display, MQTT)
      xQueueSend(ecgDataQueue, &ecgData, 0);
      
      // Debug output
      Serial.print("ECG Task: Heart Rate = ");
      Serial.print(heartRate);
      Serial.print(" BPM, Raw Value = ");
      Serial.println(ecgValue);
    }
  }
}

bool isValidEcgSignal() {
  // Check lead-off detection pins if connected
  // If either LO+ or LO- is HIGH, the leads are not properly connected
  if (digitalRead(ECG_LO_POS_PIN) == HIGH || digitalRead(ECG_LO_NEG_PIN) == HIGH) {
    return false;
  }
  return true;
}

int calculateHeartRate(int *samples, int count) {
  // This function could implement a more sophisticated heart rate calculation
  // Currently, we're using the simpler peak detection in the main task loop
  return heartRate;
}
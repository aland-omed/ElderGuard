/**
 * ElderGuard - ECG & Heart Rate Task Implementation
 * 
 * This file implements the ECG monitoring task using the AD8232 module.
 * It samples ECG data, calculates heart rate, and signals other tasks
 * through semaphores.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "../include/ecg_task.h"
#include "../include/config.h"
#include "../include/globals.h"

// Constants for ECG processing
#define ECG_BUFFER_SIZE 250              // 5 seconds at 50Hz
#define SAMPLE_INTERVAL_MS (1000 / ECG_SAMPLE_FREQUENCY_HZ) // Time between samples

// Advanced ECG processing parameters
#define PEAK_DETECTION_THRESHOLD 3650    // Initial threshold for R-peak detection
#define RR_MIN_LIMIT 300                 // Minimum RR interval (200 BPM max)
#define RR_MAX_LIMIT 1200                // Maximum RR interval (50 BPM min)
#define QRS_MIN_WIDTH 10                 // Minimum width of QRS complex in ms
#define QRS_MAX_WIDTH 150                // Maximum width of QRS complex in ms
#define LEARNING_FACTOR 0.2              // For adaptive threshold

// Make buffer and index accessible to other files
int ecgBuffer[ECG_BUFFER_SIZE];
int bufferIndex = 0;

// Variables for heart rate calculation
int heartRate = 0;
int lastValidHeartRate = 0;
unsigned long lastPeakTime = 0;
int adaptiveThreshold = PEAK_DETECTION_THRESHOLD;
int noiseLevel = 0;
int signalLevel = 0;

// RR interval storage
#define RR_BUFFER_SIZE 8
unsigned long rrIntervals[RR_BUFFER_SIZE];
int rrIndex = 0;

void ecgTask(void *pvParameters) {
  // Setup ADC for ECG input
  adc1_config_width(ADC_WIDTH_BIT_12);                   // 12-bit resolution (0-4095)
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_12);
  
  // Configure lead-off detection pins if used
  pinMode(ECG_LO_POS_PIN, INPUT);
  pinMode(ECG_LO_NEG_PIN, INPUT);
  
  Serial.println("ECG Task: Started with improved heart rate calculation");

  // Variables for task timing
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLE_INTERVAL_MS);
  xLastWakeTime = xTaskGetTickCount();
  
  // Variables for data publishing
  unsigned long lastDataUpdate = 0;
  unsigned long lastValidSignalTime = 0;
  
  // Initialize RR intervals
  for (int i = 0; i < RR_BUFFER_SIZE; i++) {
    rrIntervals[i] = 0;
  }
  
  // QRS detection state variables
  bool inQRS = false;
  unsigned long qrsStartTime = 0;
  int qrsPeak = 0;
  
  // Main task loop
  while (true) {
    // Ensure task runs at consistent frequency
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    unsigned long currentTime = millis();
    
    // Read ECG value
    int rawEcgValue = adc1_get_raw(ADC1_CHANNEL_0);
    
    // Store in circular buffer
    ecgBuffer[bufferIndex] = rawEcgValue;
    bufferIndex = (bufferIndex + 1) % ECG_BUFFER_SIZE;
    
    // Check for valid signal
    bool validSignal = isValidEcgSignal();
    
    // Apply basic filter (5-point moving average)
    int filteredValue = 0;
    for (int i = 0; i < 5; i++) {
      int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
      filteredValue += ecgBuffer[idx];
    }
    filteredValue /= 5;
    
    // Update signal quality metrics
    if (validSignal) {
      lastValidSignalTime = currentTime;
      
      // QRS complex detection - state machine
      if (!inQRS && filteredValue > adaptiveThreshold) {
        // Entering QRS complex
        inQRS = true;
        qrsStartTime = currentTime;
        qrsPeak = filteredValue;
      } else if (inQRS) {
        // In QRS complex - track peak
        if (filteredValue > qrsPeak) {
          qrsPeak = filteredValue;
        }
        
        // Exit QRS complex if signal drops below threshold
        if (filteredValue < adaptiveThreshold) {
          inQRS = false;
          unsigned long qrsDuration = currentTime - qrsStartTime;
          
          // Validate QRS width
          if (qrsDuration >= QRS_MIN_WIDTH && qrsDuration <= QRS_MAX_WIDTH) {
            // Valid QRS complex detected
            
            // Calculate RR interval
            if (lastPeakTime > 0) {
              unsigned long rrInterval = currentTime - lastPeakTime;
              
              // Validate RR interval physiologically plausible
              if (rrInterval >= RR_MIN_LIMIT && rrInterval <= RR_MAX_LIMIT) {
                // Store RR interval
                rrIntervals[rrIndex] = rrInterval;
                rrIndex = (rrIndex + 1) % RR_BUFFER_SIZE;
                
                // Update signal and noise levels for adaptive threshold
                signalLevel = (int)(LEARNING_FACTOR * qrsPeak + (1 - LEARNING_FACTOR) * signalLevel);
                
                // Calculate heart rate from recent RR intervals
                unsigned long rrSum = 0;
                int validRR = 0;
                
                for (int i = 0; i < RR_BUFFER_SIZE; i++) {
                  if (rrIntervals[i] > 0) {
                    rrSum += rrIntervals[i];
                    validRR++;
                  }
                }
                
                if (validRR > 0) {
                  unsigned long avgRR = rrSum / validRR;
                  int newHeartRate = 60000 / avgRR;
                  
                  // Apply smoothing to heart rate
                  if (heartRate == 0) {
                    heartRate = newHeartRate;
                  } else {
                    heartRate = (int)(0.7 * heartRate + 0.3 * newHeartRate);
                  }
                  
                  // Store last valid heart rate
                  lastValidHeartRate = heartRate;
                  
                  Serial.printf("ECG Task: QRS detected - HR: %d, RR: %lu ms\n", 
                               heartRate, rrInterval);
                }
              }
            }
            
            // Update last peak time
            lastPeakTime = currentTime;
          }
          
          // Update adaptive threshold
          if (signalLevel > 0) {
            adaptiveThreshold = noiseLevel + (signalLevel - noiseLevel) / 2;
          }
        }
      } else {
        // Outside QRS, update noise level
        noiseLevel = (int)(LEARNING_FACTOR * filteredValue + (1 - LEARNING_FACTOR) * noiseLevel);
      }
    } else {
      // No valid signal - reset QRS detection
      inQRS = false;
      
      // If signal lost for more than 3 seconds, clear heart rate
      if (currentTime - lastValidSignalTime > 3000) {
        // Keep last valid value but mark as stale
        if (heartRate != 0) {
          Serial.println("ECG Task: Signal lost");
          heartRate = 0;
        }
      }
    }
    
    // Update shared data periodically
    if (currentTime - lastDataUpdate >= MQTT_PUBLISH_INTERVAL_MS) {
      lastDataUpdate = currentTime;
      
      // Use heart rate = 0 to indicate no valid signal after extended period
      // Allow displaying the previous valid heart rate for up to 5 seconds after signal loss
      int reportedHeartRate;
      bool reportedSignalValid;
      
      if (validSignal && heartRate > 0) {
        // We have a valid signal and heart rate - use current values
        reportedHeartRate = heartRate;
        reportedSignalValid = true;
        lastValidHeartRate = heartRate;
      } else if (currentTime - lastValidSignalTime < 5000 && lastValidHeartRate > 0) {
        // Signal lost, but within acceptable window to show last valid reading
        reportedHeartRate = lastValidHeartRate;
        reportedSignalValid = false; // Report honestly that signal is invalid
        
        // Debug output for using last valid heart rate
        static unsigned long lastFallbackDebugTime = 0;
        if (currentTime - lastFallbackDebugTime > 3000) {
          lastFallbackDebugTime = currentTime;
          Serial.printf("ECG Task: Using last valid heart rate: %d BPM (signal lost for %lu ms)\n", 
                       lastValidHeartRate, currentTime - lastValidSignalTime);
        }
      } else {
        // Signal lost for too long
        reportedHeartRate = 0;
        reportedSignalValid = false;
      }
      
      // Take mutex before updating shared variables
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Update shared ECG data
        currentEcgData.rawValue = rawEcgValue;
        currentEcgData.heartRate = reportedHeartRate;
        currentEcgData.validSignal = reportedSignalValid;
        currentEcgData.timestamp = currentTime;
        ecgDataUpdated = true;
        
        // Release mutex
        xSemaphoreGive(displayMutex);
        
        // Signal other tasks that ECG data is updated
        xSemaphoreGive(ecgDataSemaphore);
        
        // Debug output every 5 seconds
        static unsigned long lastDebugOutput = 0;
        if (currentTime - lastDebugOutput >= 5000) {
          lastDebugOutput = currentTime;
          Serial.printf("ECG Task: Heart Rate = %d BPM, Signal: %s, Threshold: %d\n", 
                       reportedHeartRate, 
                       reportedSignalValid ? "Valid" : "Invalid", 
                       adaptiveThreshold);
        }
      }
    }
  }
}

bool isValidEcgSignal() {
  // Check lead-off detection pins if connected
  // If either LO+ or LO- is HIGH, the leads are not properly connected
  if (digitalRead(ECG_LO_POS_PIN) == HIGH || digitalRead(ECG_LO_NEG_PIN) == HIGH) {
    // Debug output when lead-off is detected
    static unsigned long lastLeadOffDebugTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastLeadOffDebugTime > 2000) {
      lastLeadOffDebugTime = currentTime;
      Serial.println("ECG Task: Lead-off condition detected!");
    }
    return false;
  }
  
  // Check if we have reasonable values - wider range for different AD8232 configurations
  int latest = ecgBuffer[bufferIndex == 0 ? ECG_BUFFER_SIZE - 1 : bufferIndex - 1];
  if (latest < 10 || latest > 4080) {
    static unsigned long lastValueDebugTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastValueDebugTime > 2000) {
      lastValueDebugTime = currentTime;
      Serial.printf("ECG Task: Raw value out of range: %d\n", latest);
    }
    return false;
  }
  
  // Check for signal stability - use a less strict condition
  // to accommodate various AD8232 configurations
  int min_val = 4095;
  int max_val = 0;
  
  // Look at last 5 samples to check variance
  for (int i = 0; i < 5; i++) {
    int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
    int val = ecgBuffer[idx];
    
    if (val < min_val) min_val = val;
    if (val > max_val) max_val = val;
  }
  
  int variance = max_val - min_val;
  
  // Either complete flatline or extreme noise indicates invalid signal
  // Made more permissive to handle different sensor behaviors
  if (variance < 5 || variance > 3000) {
    static unsigned long lastStabilityDebugTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastStabilityDebugTime > 2000) {
      lastStabilityDebugTime = currentTime;
      Serial.printf("ECG Task: Signal stability issue, variance: %d\n", variance);
    }
    return false;
  }
  
  return true;
}

int calculateHeartRate(int *samples, int count) {
  // In case we need to implement a more sophisticated algorithm
  // Currently handled in the main task loop
  return heartRate;
}
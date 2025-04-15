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
#include "../include/mqtt_task.h"

// Constants for ECG processing
#define ECG_BUFFER_SIZE 250              // 5 seconds at 50Hz
#define SAMPLE_INTERVAL_MS (1000 / ECG_SAMPLE_FREQUENCY_HZ) // Time between samples

// Advanced ECG processing parameters - adjusted for AD8232
#define PEAK_DETECTION_THRESHOLD 2700    // Initial threshold for R-peak detection - adjusted for AD8232
#define RR_MIN_LIMIT 300                 // Minimum RR interval (200 BPM max)
#define RR_MAX_LIMIT 1500                // Maximum RR interval (40 BPM min)
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

// Variable to track lead-off status
bool leadsConnected = false;
unsigned long lastHeartRateChangeTime = 0;

void ecgTask(void *pvParameters) {
  // Setup ADC for ECG input
  adc1_config_width(ADC_WIDTH_BIT_12);                   // 12-bit resolution (0-4095)
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11); // Adjusted for typical AD8232 output
  
  // Configure lead-off detection pins if used
  pinMode(ECG_LO_POS_PIN, INPUT);
  pinMode(ECG_LO_NEG_PIN, INPUT);
  
  Serial.println("ECG Task: Started with optimized heart rate detection");

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
  
  // Baseline tracking variables
  int baseline = 2048; // Start at midpoint
  
  // Buffer for tracking peak-to-peak amplitude
  #define AMPLITUDE_BUFFER_SIZE 5
  int recentAmplitudes[AMPLITUDE_BUFFER_SIZE] = {0};
  int amplitudeIndex = 0;
  int averageAmplitude = 500; // Initial guess
  
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
    
    // Check lead connection status by reading dedicated pins
    leadsConnected = !(digitalRead(ECG_LO_POS_PIN) == HIGH || digitalRead(ECG_LO_NEG_PIN) == HIGH);
    
    // If leads disconnected, reset heart rate
    if (!leadsConnected) {
      if (heartRate != 0) {
        Serial.println("ECG Task: Leads disconnected, resetting heart rate");
        heartRate = 0;
      }
      // Skip further processing
      
      // Update shared data periodically even when disconnected
      if (currentTime - lastDataUpdate >= MQTT_PUBLISH_INTERVAL_MS) {
        lastDataUpdate = currentTime;
        
        // Take mutex before updating shared variables
        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          // Update shared ECG data
          currentEcgData.rawValue = rawEcgValue;
          currentEcgData.heartRate = 0;
          currentEcgData.validSignal = false;
          currentEcgData.timestamp = currentTime;
          ecgDataUpdated = true;
          
          // Release mutex
          xSemaphoreGive(displayMutex);
          
          // Signal other tasks that ECG data is updated
          xSemaphoreGive(ecgDataSemaphore);
        }
      }
      
      vTaskDelay(pdMS_TO_TICKS(10)); // Small delay
      continue; // Skip to next iteration
    }
    
    // Apply basic filter (5-point moving average)
    int filteredValue = 0;
    for (int i = 0; i < 5; i++) {
      int idx = (bufferIndex - i - 1 + ECG_BUFFER_SIZE) % ECG_BUFFER_SIZE;
      filteredValue += ecgBuffer[idx];
    }
    filteredValue /= 5;
    
    // Update baseline with slow tracking (low pass filter)
    baseline = (baseline * 99 + filteredValue) / 100;
    
    // Calculate the deviation from baseline
    int deviation = filteredValue - baseline;
    
    // Update adaptive threshold based on recent signal
    if (currentTime - lastPeakTime > 1500) { // If no peaks for 1.5 seconds
      // Reduce threshold to be more sensitive
      adaptiveThreshold = baseline + averageAmplitude / 2;
    }
    
    // QRS complex detection - state machine
    if (!inQRS && deviation > adaptiveThreshold - baseline) {
      // Entering QRS complex
      inQRS = true;
      qrsStartTime = currentTime;
      qrsPeak = filteredValue;
    } else if (inQRS) {
      // In QRS complex - track peak
      if (filteredValue > qrsPeak) {
        qrsPeak = filteredValue;
      }
      
      // Exit QRS complex if signal drops below threshold or too much time passed
      if (deviation < (adaptiveThreshold - baseline) / 2 || (currentTime - qrsStartTime > QRS_MAX_WIDTH)) {
        inQRS = false;
        unsigned long qrsDuration = currentTime - qrsStartTime;
        
        // Don't validate QRS width during initial setup
        if (currentTime < 3000 || (qrsDuration >= QRS_MIN_WIDTH && qrsDuration <= QRS_MAX_WIDTH)) {
          // Valid QRS complex detected
          
          // Update amplitude tracking
          int peakAmplitude = qrsPeak - baseline;
          recentAmplitudes[amplitudeIndex] = peakAmplitude;
          amplitudeIndex = (amplitudeIndex + 1) % AMPLITUDE_BUFFER_SIZE;
          
          // Recalculate average amplitude
          int sumAmplitude = 0;
          int countAmplitude = 0;
          for (int i = 0; i < AMPLITUDE_BUFFER_SIZE; i++) {
            if (recentAmplitudes[i] > 0) {
              sumAmplitude += recentAmplitudes[i];
              countAmplitude++;
            }
          }
          
          if (countAmplitude > 0) {
            averageAmplitude = sumAmplitude / countAmplitude;
            // Update adaptive threshold based on amplitude
            adaptiveThreshold = baseline + averageAmplitude / 2;
          }
          
          // Calculate RR interval if we have a previous peak
          if (lastPeakTime > 0) {
            unsigned long rrInterval = currentTime - lastPeakTime;
            
            // Validate RR interval is physiologically plausible
            if (rrInterval >= RR_MIN_LIMIT && rrInterval <= RR_MAX_LIMIT) {
              // Store RR interval
              rrIntervals[rrIndex] = rrInterval;
              rrIndex = (rrIndex + 1) % RR_BUFFER_SIZE;
              
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
                  // Use stronger smoothing for more stable readings
                  heartRate = (int)(0.8 * heartRate + 0.2 * newHeartRate);
                }
                
                // Store last valid heart rate
                lastValidHeartRate = heartRate;
                lastHeartRateChangeTime = currentTime;
                
                // Debug output only when heart rate changes significantly
                static int lastReportedHR = 0;
                if (abs(heartRate - lastReportedHR) >= 3) {
                  lastReportedHR = heartRate;
                  Serial.printf("ECG Task: QRS detected - HR: %d BPM, RR: %lu ms, Amp: %d\n", 
                               heartRate, rrInterval, averageAmplitude);
                }
              }
            }
          }
          
          // Update last peak time
          lastPeakTime = currentTime;
          lastValidSignalTime = currentTime;
        }
      }
    }
    
    // If we've gone too long without detecting heart rate, reset it
    if (heartRate > 0 && currentTime - lastHeartRateChangeTime > 8000) {
      Serial.println("ECG Task: No heartbeats detected for 8 seconds, resetting heart rate");
      heartRate = 0;
    }
    
    // Update shared data periodically
    if (currentTime - lastDataUpdate >= MQTT_PUBLISH_INTERVAL_MS) {
      lastDataUpdate = currentTime;
      
      // Create a diagnostic string for debugging
      char diagString[80];
      sprintf(diagString, "B:%d, T:%d, R:%d, A:%d", 
              baseline, adaptiveThreshold, rawEcgValue, averageAmplitude);
      
      // Take mutex before updating shared variables
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Update shared ECG data
        currentEcgData.rawValue = rawEcgValue;
        currentEcgData.heartRate = heartRate;
        currentEcgData.validSignal = (heartRate > 0);
        currentEcgData.timestamp = currentTime;
        ecgDataUpdated = true;
        
        // Release mutex
        xSemaphoreGive(displayMutex);
        
        // Signal other tasks that ECG data is updated
        xSemaphoreGive(ecgDataSemaphore);
        
        // Publish ECG data to MQTT
        publishEcgData(rawEcgValue, heartRate, (heartRate > 0));
        
        // Debug output every 5 seconds
        static unsigned long lastDebugOutput = 0;
        if (currentTime - lastDebugOutput >= 5000) {
          lastDebugOutput = currentTime;
          Serial.printf("ECG Task: Heart Rate = %d BPM, Signal: %s, %s\n", 
                       heartRate, 
                       heartRate > 0 ? "Valid" : "Invalid",
                       diagString);
        }
      }
    }
  }
}

bool isValidEcgSignal() {
  // Simply check lead connection
  return leadsConnected;
}

int calculateHeartRate(int *samples, int count) {
  // This is now handled in the main task loop
  return heartRate;
}
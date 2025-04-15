/**
 * ElderGuard - Fall Detection Task Implementation
 * 
 * This file implements the fall detection task using the MPU6050 accelerometer.
 * It detects falls and sends alerts immediately through HTTP when a fall is detected.
 * This task has the highest priority in the system.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "../include/fall_detection_task.h"
#include "../include/config.h"

// External queue handle declared in main.cpp
extern QueueHandle_t fallDetectionQueue;
extern QueueHandle_t audioCommandQueue;

// MPU6050 sensor object
Adafruit_MPU6050 mpu;

// Configuration parameters
struct FallConfig {
  // Fall detection thresholds
  float freefallThreshold = 4.0;          // m/s² - Low acceleration indicates freefall
  float impactThreshold = 24.0;           // m/s² - High acceleration indicates impact
  float orientationChangeThreshold = 45.0; // degrees - Orientation change after fall
  
  // Timing parameters
  unsigned long minFreefallDuration = 150;  // ms - Minimum time in freefall state
  unsigned long maxFreefallWindow = 1000;   // ms - Maximum time between freefall and impact
  unsigned long fallResetTime = 3000;       // ms - Time before system resets after fall
};

// Orientation tracking structure
struct Orientation {
  float pitch = 0.0;
  float roll = 0.0;
  float yaw = 0.0;
  float baselinePitch = 0.0;
  float baselineRoll = 0.0;
  float baselineYaw = 0.0;
};

// Global variables
FallConfig config;
Orientation orientation;
FallState currentState = MONITORING;
unsigned long stateStartTime = 0;
unsigned long fallDetectedTime = 0;
float peakAcceleration = 0.0;
float minAcceleration = 9.8;
const float ALPHA = 0.8; // Low-pass filter coefficient

void fallDetectionTask(void *pvParameters) {
  Serial.println("Fall Detection Task: Started");
  
  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
  
  // Configure sensor ranges
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  Serial.println("MPU6050 initialized successfully");
  
  // Calibrate the accelerometer to establish a baseline
  calibrateAccelerometer();
  
  // Variables for task timing
  TickType_t xLastWakeTime;
  const TickType_t xFrequency = pdMS_TO_TICKS(1000 / FALL_DETECTION_SAMPLE_RATE_HZ);
  xLastWakeTime = xTaskGetTickCount();
  
  // Main task loop
  while (true) {
    // Ensure task runs at consistent frequency
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
    
    // Read accelerometer and gyroscope data
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);
    
    // Update orientation
    updateOrientation(accel, gyro);
    
    // Process fall detection algorithm
    processFallDetection(accel, gyro);
  }
}

void calibrateAccelerometer() {
  Serial.println("Calibrating baseline orientation...");
  Serial.println("Keep device stationary");
  
  // Take multiple samples for better baseline
  const int numSamples = 100;
  float pitchSum = 0, rollSum = 0, yawSum = 0;
  
  for (int i = 0; i < numSamples; i++) {
    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);
    
    // Calculate orientation from this sample
    float ax = accel.acceleration.x;
    float ay = accel.acceleration.y;
    float az = accel.acceleration.z;
    
    float pitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
    float roll = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;
    float yaw = atan2(sqrt(ax * ax + ay * ay), az) * 180.0 / PI;
    
    pitchSum += pitch;
    rollSum += roll;
    yawSum += yaw;
    
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  
  // Set baseline orientation values
  orientation.baselinePitch = pitchSum / numSamples;
  orientation.baselineRoll = rollSum / numSamples;
  orientation.baselineYaw = yawSum / numSamples;
  
  Serial.println("Calibration complete");
  Serial.print("Baseline pitch: "); Serial.println(orientation.baselinePitch);
  Serial.print("Baseline roll: "); Serial.println(orientation.baselineRoll);
  Serial.print("Baseline yaw: "); Serial.println(orientation.baselineYaw);
}

void updateOrientation(sensors_event_t accel, sensors_event_t gyro) {
  // Calculate raw orientation values
  float ax = accel.acceleration.x;
  float ay = accel.acceleration.y;
  float az = accel.acceleration.z;
  
  float newPitch = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  float newRoll = atan2(ay, sqrt(ax * ax + az * az)) * 180.0 / PI;
  float newYaw = atan2(sqrt(ax * ax + ay * ay), az) * 180.0 / PI;
  
  // Apply low-pass filter to smooth data
  orientation.pitch = ALPHA * orientation.pitch + (1 - ALPHA) * newPitch;
  orientation.roll = ALPHA * orientation.roll + (1 - ALPHA) * newRoll;
  orientation.yaw = ALPHA * orientation.yaw + (1 - ALPHA) * newYaw;
}

float calculateAccelerationMagnitude(float x, float y, float z) {
  return sqrt(x * x + y * y + z * z);
}

void processFallDetection(sensors_event_t accel, sensors_event_t gyro) {
  unsigned long currentTime = millis();
  
  // Calculate acceleration magnitude
  float accMagnitude = calculateAccelerationMagnitude(
    accel.acceleration.x, 
    accel.acceleration.y, 
    accel.acceleration.z
  );
  
  // Update min/max values for analysis
  if (accMagnitude > peakAcceleration) {
    peakAcceleration = accMagnitude;
  }
  if (accMagnitude < minAcceleration) {
    minAcceleration = accMagnitude;
  }
  
  // Fall detection state machine
  switch (currentState) {
    case MONITORING:
      // Look for potential freefall condition
      if (accMagnitude < config.freefallThreshold) {
        currentState = POTENTIAL_FALL;
        stateStartTime = currentTime;
        peakAcceleration = 0;
        minAcceleration = accMagnitude;
        
        Serial.println("Fall Detection: Potential fall detected - Low acceleration");
      }
      break;
      
    case POTENTIAL_FALL:
      // Confirm freefall persists for minimum duration
      if (currentTime - stateStartTime >= config.minFreefallDuration) {
        // Look for impact
        if (accMagnitude > config.impactThreshold) {
          currentState = IMPACT_DETECTED;
          stateStartTime = currentTime;
          
          Serial.print("Fall Detection: Impact detected - Acceleration: ");
          Serial.println(accMagnitude);
        }
      }
      
      // Reset if no impact detected within window
      if (currentTime - stateStartTime > config.maxFreefallWindow) {
        currentState = MONITORING;
        Serial.println("Fall Detection: Potential fall timeout - No impact detected");
      }
      break;
      
    case IMPACT_DETECTED:
      {
        // Determine fall direction from orientation changes
        float pitchChange = orientation.pitch - orientation.baselinePitch;
        float rollChange = orientation.roll - orientation.baselineRoll;
        
        bool significantOrientationChange = 
          fabs(pitchChange) > config.orientationChangeThreshold || 
          fabs(rollChange) > config.orientationChangeThreshold;
        
        if (significantOrientationChange) {
          currentState = FALL_CONFIRMED;
          fallDetectedTime = currentTime;
          
          // Report fall event
          reportFallEvent(pitchChange, rollChange);
          
          // Trigger audio alert
          AudioCommand audioCmd;
          audioCmd.fileNumber = AUDIO_FALL_DETECTED;
          audioCmd.repeatCount = 3;
          audioCmd.volume = 30; // Max volume
          xQueueSend(audioCommandQueue, &audioCmd, 0);
        } else {
          // No significant orientation change - likely not a fall
          currentState = MONITORING;
          Serial.println("Fall Detection: No significant orientation change - Not a fall");
        }
      }
      break;
      
    case FALL_CONFIRMED:
      // Wait for the reset time before returning to monitoring
      if (currentTime - fallDetectedTime > config.fallResetTime) {
        currentState = MONITORING;
        peakAcceleration = 0;
        minAcceleration = 9.8;
        Serial.println("Fall Detection: Reset to monitoring state");
      }
      break;
  }
}

void reportFallEvent(float pitchChange, float rollChange) {
  Serial.println("\n!!! FALL DETECTED !!!");
  
  // Determine fall direction
  bool forwardFall = pitchChange < -config.orientationChangeThreshold;
  bool backwardFall = pitchChange > config.orientationChangeThreshold;
  bool leftFall = rollChange < -config.orientationChangeThreshold;
  bool rightFall = rollChange > config.orientationChangeThreshold;
  
  // Report fall direction
  Serial.print("Fall Direction: ");
  if (forwardFall) Serial.print("Forward ");
  if (backwardFall) Serial.print("Backward ");
  if (leftFall) Serial.print("Left ");
  if (rightFall) Serial.print("Right ");
  Serial.println();
  
  // Report fall metrics
  Serial.print("Peak acceleration: "); Serial.print(peakAcceleration); Serial.println(" m/s²");
  Serial.print("Min acceleration: "); Serial.print(minAcceleration); Serial.println(" m/s²");
  
  // Orientation after fall
  Serial.print("Final orientation - Pitch: "); Serial.print(orientation.pitch);
  Serial.print(", Roll: "); Serial.println(orientation.roll);
  
  // Create fall event structure
  FallEvent fallEvent;
  fallEvent.fallDetected = true;
  fallEvent.acceleration = peakAcceleration;
  fallEvent.orientation[0] = orientation.pitch;
  fallEvent.orientation[1] = orientation.roll;
  fallEvent.orientation[2] = orientation.yaw;
  fallEvent.timestamp = millis();
  fallEvent.fallSeverity = map(peakAcceleration, config.impactThreshold, 40.0, 1, 10);
  
  // Send fall event to queue
  xQueueSend(fallDetectionQueue, &fallEvent, 0);
  
  Serial.println("Fall event sent to queue - Would send HTTP alert immediately");
}
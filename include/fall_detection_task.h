/**
 * ElderGuard - Fall Detection Task
 * 
 * This file declares the fall detection task interface for detecting
 * falls using the MPU6050 accelerometer module.
 */

#ifndef FALL_DETECTION_TASK_H
#define FALL_DETECTION_TASK_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include "config.h"

// Function prototypes
void fallDetectionTask(void *pvParameters);
bool detectFall();
void calibrateAccelerometer();
float calculateAccelerationMagnitude(float x, float y, float z);
void updateOrientation(sensors_event_t accel, sensors_event_t gyro);
void processFallDetection(sensors_event_t accel, sensors_event_t gyro);
void reportFallEvent(float pitchChange, float rollChange);

// Fall detection states
enum FallState {
  MONITORING,        // Normal state, checking for falls
  POTENTIAL_FALL,    // Low acceleration detected (potential free-fall)
  IMPACT_DETECTED,   // High acceleration detected (potential impact)
  ORIENTATION_CHECK, // Checking orientation change
  FALL_CONFIRMED     // Fall confirmed, sending alerts
};

#endif // FALL_DETECTION_TASK_H
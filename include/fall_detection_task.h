/**
 * ElderGuard - Fall Detection Task
 * 
 * This file contains the declarations for fall detection task
 * using the MPU6050 accelerometer.
 */

#ifndef FALL_DETECTION_TASK_H
#define FALL_DETECTION_TASK_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include "config.h"
#include "globals.h"

// Function prototypes
void fallDetectionTask(void *pvParameters);
bool detectFall(float *acceleration, float *orientation);
int assessFallSeverity(float impact);
void calibrateAccelerometer();
void updateOrientation(sensors_event_t accel, sensors_event_t gyro);
void processFallDetection(sensors_event_t accel, sensors_event_t gyro);
void reportFallEvent(float pitchChange, float rollChange);
float calculateAccelerationMagnitude(float x, float y, float z);

#endif // FALL_DETECTION_TASK_H
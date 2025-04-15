/**
 * ElderGuard - GPS Tracking Task
 * 
 * This file contains the declarations for GPS tracking task
 * using the NEO-6M GPS module.
 */

#ifndef GPS_TASK_H
#define GPS_TASK_H

#include <Arduino.h>
#include "config.h"
#include "globals.h"

// Function prototypes
void gpsTask(void *pvParameters);
bool parseGpsData();
void initializeGps();
void updateGpsData(unsigned long timestamp);
void printGpsDebugInfo();

#endif // GPS_TASK_H
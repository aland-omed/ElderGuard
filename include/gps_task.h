/**
 * ElderGuard - GPS Task
 * 
 * This file declares the GPS task interface for location tracking
 * using the GY-NEO6MV2 GPS module.
 */

#ifndef GPS_TASK_H
#define GPS_TASK_H

#include <Arduino.h>
#include "config.h"

// Function prototypes
void gpsTask(void *pvParameters);
bool parseGpsData();
void processGpsData();
void updateGpsData(unsigned long timestamp);
void printGpsDebugInfo();

#endif // GPS_TASK_H
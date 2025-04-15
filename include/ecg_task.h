/**
 * ElderGuard - ECG & Heart Rate Monitoring Task
 * 
 * This file declares the ECG task interface for monitoring heart rate
 * using the AD8232 ECG module.
 */

#ifndef ECG_TASK_H
#define ECG_TASK_H

#include <Arduino.h>
#include "config.h"

// Function prototypes
void ecgTask(void *pvParameters);
int calculateHeartRate(int *samples, int count);
bool isValidEcgSignal();

#endif // ECG_TASK_H
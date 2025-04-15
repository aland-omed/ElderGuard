/**
 * ElderGuard - Medication Task
 * 
 * This file declares the medication reminder task interface.
 */

#ifndef MEDICATION_TASK_H
#define MEDICATION_TASK_H

#include <Arduino.h>
#include <time.h>
#include "config.h"

// Function prototypes
void medicationTask(void *pvParameters);
void checkMedications(struct tm *currentTime);
void triggerMedicationReminder(const char* medicationName);

#endif // MEDICATION_TASK_H
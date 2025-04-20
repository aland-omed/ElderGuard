/**
 * ElderGuard - Medication Reminder Task
 * 
 * This file contains declarations for the medication reminder task
 * that manages scheduled medications for the ElderGuard system.
 */

#ifndef MEDICATION_TASK_H
#define MEDICATION_TASK_H

#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "globals.h"

// Function prototypes
void medicationTask(void *pvParameters);
void triggerMedicationReminder(const char* medicationName);
void triggerMedicationAdvanceReminder(const char* medicationName);
void updateUpcomingMedicationDisplay();
void playMedicationSound(const char* medicationName, bool isAdvance);

#endif // MEDICATION_TASK_H
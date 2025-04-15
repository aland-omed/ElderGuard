/**
 * ElderGuard - Global Variables and Semaphores
 * 
 * This file contains all global variables and semaphores used for
 * communication between tasks in the ElderGuard system.
 */

#ifndef GLOBALS_H
#define GLOBALS_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config.h"

// Semaphores for task synchronization
extern SemaphoreHandle_t displayMutex;
extern SemaphoreHandle_t ecgDataSemaphore;
extern SemaphoreHandle_t gpsDataSemaphore;
extern SemaphoreHandle_t fallDetectionSemaphore;
extern SemaphoreHandle_t medicationSemaphore;
extern SemaphoreHandle_t audioCommandSemaphore;

// Shared ECG Data
extern volatile EcgData currentEcgData;
extern volatile bool ecgDataUpdated;

// Shared GPS Data
extern volatile GpsData currentGpsData;
extern volatile bool gpsDataUpdated;

// Shared Fall Detection Data
extern volatile FallEvent currentFallEvent;
extern volatile bool fallDetectionUpdated;

// Shared Medication Data
extern volatile MedicationReminder currentMedicationReminder;
extern volatile bool medicationReminderUpdated;

// Shared Audio Command Data
extern volatile AudioCommand currentAudioCommand;
extern volatile bool audioCommandUpdated;

#endif // GLOBALS_H
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
extern SemaphoreHandle_t wifiStatusSemaphore;
extern SemaphoreHandle_t timeStatusSemaphore;

// Shared ECG Data
extern volatile EcgData currentEcgData;
extern volatile bool ecgDataUpdated;

// Shared GPS Data
extern volatile GpsData currentGpsData;
extern volatile bool gpsDataUpdated;

// Shared Fall Detection Data
extern volatile FallEvent currentFallEvent;
extern volatile bool fallDetectionUpdated;

// Upcoming Medication Information
typedef struct {
    char name[32];         // Medication name
    char timeStr[6];       // Time string (HH:MM)
    bool available;        // Whether there is an upcoming medication
} UpcomingMedication;

// Shared Medication Data
extern volatile MedicationReminder currentMedicationReminder;
extern volatile bool medicationReminderUpdated;
extern volatile UpcomingMedication upcomingMedication;
extern volatile bool upcomingMedicationUpdated;

// Shared Audio Command Data
extern volatile AudioCommand currentAudioCommand;
extern volatile bool audioCommandUpdated;

// Shared WiFi Status Data
extern volatile WiFiStatus currentWiFiStatus;
extern volatile bool wifiStatusUpdated;

// Shared Time Status Data
extern volatile TimeStatus currentTimeStatus;
extern volatile bool timeStatusUpdated;

// Flag for display update requests
extern volatile bool needsDisplayUpdate;

#endif // GLOBALS_H
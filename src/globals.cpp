/**
 * ElderGuard - Global Variables and Semaphores Implementation
 * 
 * This file implements all global variables and semaphores used for
 * communication between tasks in the ElderGuard system.
 */

#include "../include/globals.h"

// Semaphores for task synchronization
SemaphoreHandle_t displayMutex;
SemaphoreHandle_t ecgDataSemaphore;
SemaphoreHandle_t gpsDataSemaphore;
SemaphoreHandle_t fallDetectionSemaphore;
SemaphoreHandle_t medicationSemaphore;
SemaphoreHandle_t audioCommandSemaphore;
SemaphoreHandle_t wifiStatusSemaphore;
SemaphoreHandle_t timeStatusSemaphore;

// Shared ECG Data
volatile EcgData currentEcgData;
volatile bool ecgDataUpdated = false;

// Shared GPS Data
volatile GpsData currentGpsData;
volatile bool gpsDataUpdated = false;

// Shared Fall Detection Data
volatile FallEvent currentFallEvent;
volatile bool fallDetectionUpdated = false;

// Shared Medication Data
volatile MedicationReminder currentMedicationReminder;
volatile bool medicationReminderUpdated = false;

// Shared Audio Command Data
volatile AudioCommand currentAudioCommand;
volatile bool audioCommandUpdated = false;

// Shared WiFi Status Data
volatile WiFiStatus currentWiFiStatus;
volatile bool wifiStatusUpdated = false;

// Shared Time Status Data
volatile TimeStatus currentTimeStatus;
volatile bool timeStatusUpdated = false;

// Flag for display update requests
volatile bool needsDisplayUpdate = false;
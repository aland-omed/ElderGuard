/**
 * ElderGuard - Medication Task Implementation
 * 
 * This file implements the medication reminder functionality that receives
 * medication times from an API and triggers alerts when it's time to take medications.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include "../include/medication_task.h"
#include "../include/config.h"
#include "../include/globals.h"

// Maximum number of medications to track
#define MAX_MEDICATIONS 10

// Medication schedule structure
struct MedicationSchedule {
  char name[32];              // Medication name
  tm scheduleTime;            // Scheduled time
  bool active;                // Whether this slot is active
  bool reminded;              // Whether a reminder has been sent today
  int repeatDays;             // Bitmap of days to repeat (bit 0 = Sunday, etc.)
};

// Array of medication schedules
MedicationSchedule medications[MAX_MEDICATIONS];

// Mock data for testing (in real implementation, this would come from API)
void populateMockMedications() {
  // Clear all medication slots
  for (int i = 0; i < MAX_MEDICATIONS; i++) {
    medications[i].active = false;
    medications[i].reminded = false;
  }
  
  // Medication 1: Morning vitamins
  strncpy(medications[0].name, "Morning Vitamins", 31);
  medications[0].active = true;
  medications[0].reminded = false;
  medications[0].repeatDays = 0b1111111; // Every day
  
  // Set time to 8:00 AM
  medications[0].scheduleTime.tm_hour = 8;
  medications[0].scheduleTime.tm_min = 0;
  
  // Medication 2: Blood pressure medication
  strncpy(medications[1].name, "BP Medication", 31);
  medications[1].active = true;
  medications[1].reminded = false;
  medications[1].repeatDays = 0b1111111; // Every day
  
  // Set time to 9:30 AM
  medications[1].scheduleTime.tm_hour = 9;
  medications[1].scheduleTime.tm_min = 30;
  
  // Medication 3: Evening medication
  strncpy(medications[2].name, "Evening Meds", 31);
  medications[2].active = true;
  medications[2].reminded = false;
  medications[2].repeatDays = 0b1111111; // Every day
  
  // Set time to 6:00 PM
  medications[2].scheduleTime.tm_hour = 18;
  medications[2].scheduleTime.tm_min = 0;
}

void medicationTask(void *pvParameters) {
  Serial.println("Medication Task: Started");
  
  // Initialize medication schedules (this would typically come from API)
  populateMockMedications();
  
  // Variables for task timing
  unsigned long lastApiCheckTime = 0;
  const unsigned long apiCheckInterval = 60 * 60 * 1000; // Check API every hour
  
  // Main task loop
  while (true) {
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Check all medications for due reminders
    checkMedications(&timeinfo);
    
    // Periodically update medication list from API (simulated)
    unsigned long currentTime = millis();
    if (currentTime - lastApiCheckTime >= apiCheckInterval) {
      lastApiCheckTime = currentTime;
      
      // In a real implementation, this would query the API
      // For now, just print a debug message
      Serial.println("Medication Task: Would check API for updated medication schedule");
    }
    
    // Check medications once per minute
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void checkMedications(struct tm *currentTime) {
  // Loop through all medication slots
  for (int i = 0; i < MAX_MEDICATIONS; i++) {
    // Skip inactive slots
    if (!medications[i].active) {
      continue;
    }
    
    // Check if the medication is scheduled for the current day
    int dayOfWeek = currentTime->tm_wday;
    bool scheduledToday = (medications[i].repeatDays & (1 << dayOfWeek)) != 0;
    
    if (!scheduledToday) {
      continue;
    }
    
    // Check if it's time for the medication
    if (currentTime->tm_hour == medications[i].scheduleTime.tm_hour && 
        currentTime->tm_min == medications[i].scheduleTime.tm_min &&
        !medications[i].reminded) {
      
      // Trigger the medication reminder
      triggerMedicationReminder(medications[i].name);
      
      // Mark as reminded
      medications[i].reminded = true;
      
      Serial.print("Medication Task: Reminder triggered for ");
      Serial.println(medications[i].name);
    }
    
    // Reset reminded flag at midnight
    if (currentTime->tm_hour == 0 && currentTime->tm_min == 0) {
      medications[i].reminded = false;
    }
  }
}

void triggerMedicationReminder(const char* medicationName) {
  // Update shared medication reminder data with mutex protection
  if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Create a local copy of the string first
    char tempName[32];
    strncpy(tempName, medicationName, sizeof(tempName) - 1);
    tempName[sizeof(tempName) - 1] = '\0';  // Ensure null termination
    
    // Copy character by character to volatile destination
    for (size_t i = 0; i < sizeof(currentMedicationReminder.name) - 1 && tempName[i] != '\0'; i++) {
      currentMedicationReminder.name[i] = tempName[i];
    }
    currentMedicationReminder.name[sizeof(currentMedicationReminder.name) - 1] = '\0';
    
    currentMedicationReminder.time = time(NULL);
    currentMedicationReminder.taken = false;
    medicationReminderUpdated = true;
    
    // Release mutex
    xSemaphoreGive(displayMutex);
    
    // Signal other tasks that medication reminder data is updated
    xSemaphoreGive(medicationSemaphore);
    
    Serial.print("Medication Task: Reminder sent for ");
    Serial.println(medicationName);
    
    // Trigger audio alert using global variables and semaphores
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Update audio command global data
      currentAudioCommand.fileNumber = AUDIO_MEDICATION;
      currentAudioCommand.repeatCount = 5; // Repeat 5 times as per requirement
      currentAudioCommand.volume = AUDIO_MAX_VOLUME; // Use maximum volume
      audioCommandUpdated = true;
      
      // Release mutex
      xSemaphoreGive(displayMutex);
      
      // Signal audio task
      xSemaphoreGive(audioCommandSemaphore);
    }
  }
}
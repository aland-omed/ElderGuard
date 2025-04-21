/**
 * ElderGuard - Medication Reminder Task Implementation
 * 
 * This task fetches medication schedules from a Laravel backend API,
 * stores them in ESP32, and reminds the patient when it's time to take medicine.
 * Features:
 * - Fetches medication schedule from Laravel
 * - Advance notification 1 minute before medication time
 * - Continuous alert for 15 seconds
 * - Displays upcoming medications
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h>
#include "../include/medication_task.h"
#include "../include/config.h"
#include "../include/globals.h"
#include "../include/wifi_task.h"

// Maximum medications to store
#define MAX_MEDS 20

// API endpoint for fetching medication schedule
const char* API_URL = "https://elderguard.codecommerce.info/api/patient/1/medication/list";

// Structure to hold medication information
struct Medication {
  int id;
  char name[32];
  int hour;
  int minute;
  bool reminded;          // Main reminder (at the scheduled time)
  bool advanceReminded;   // Advance reminder (1 minute before)
  unsigned long notificationStartTime; // When the notification started
  bool notificationActive; // If notification is currently active
};

// Array to store medications
Medication medications[MAX_MEDS];
int medicationCount = 0;

// Function prototypes
bool fetchMedicationSchedule();
void saveMedicationsToFlash();
void loadMedicationsFromFlash();
void checkMedications();
void updateUpcomingMedication();

// Upcoming medication information
char upcomingMedName[32] = "";
int upcomingMedHour = -1;
int upcomingMedMinute = -1;
bool hasUpcomingMed = false;

/**
 * Main medication task function
 */
void medicationTask(void *pvParameters) {
  Serial.println("Medication Task: Started");
  
  // Initialize SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("Medication Task: SPIFFS initialization failed");
  }
  
  // Load any previously stored medications
  loadMedicationsFromFlash();
  
  // Initial update of upcoming medication
  updateUpcomingMedication();
  
  unsigned long lastFetchTime = 0;
  unsigned long lastCheckTime = 0;
  
  // Main task loop
  while(true) {
    unsigned long currentTime = millis();
    
    // Fetch medication schedule from API every 15 minutes or on startup
    if(currentTime - lastFetchTime >= 15 * 60 * 1000 || lastFetchTime == 0) {
      if(getWiFiConnected()) {
        Serial.println("Medication Task: Fetching medication schedule");
        if(fetchMedicationSchedule()) {
          saveMedicationsToFlash();
          updateUpcomingMedication();
          lastFetchTime = currentTime;
        }
      }
    }
    
    // Check for medications to take every 5 seconds (more frequent to catch 1-minute advance)
    if(currentTime - lastCheckTime >= 5 * 1000) {
      checkMedications();
      lastCheckTime = currentTime;
    }
    
    // Handle active notifications (continuous 15-second alert)
    for(int i = 0; i < medicationCount; i++) {
      if(medications[i].notificationActive) {
        // If 15 seconds have passed, stop the notification
        if(currentTime - medications[i].notificationStartTime >= 15000) {
          medications[i].notificationActive = false;
          Serial.printf("Medication Task: Ended 15-second alert for %s\n", medications[i].name);
        }
        else if((currentTime - medications[i].notificationStartTime) % 3000 == 0) {
          // Repeat audio alert every 3 seconds during the 15-second window
          playMedicationSound(medications[i].name, false);
        }
      }
    }
    
    // Yield to other tasks
    vTaskDelay(pdMS_TO_TICKS(100)); // More responsive delay
  }
}

/**
 * Fetch medication schedule from the Laravel API
 */
bool fetchMedicationSchedule() {
  HTTPClient http;
  http.begin(API_URL);
  
  int httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    
    if(error) {
      Serial.println("Medication Task: JSON parsing failed");
      http.end();
      return false;
    }
    
    // Clear existing medications
    medicationCount = 0;
    
    // Process medication schedule data
    JsonArray data = doc["data"];
    for(JsonObject med : data) {
      if(medicationCount >= MAX_MEDS) break;
      
      int id = med["id"];
      const char* name = med["medicine_name"];
      const char* timeStr = med["scheduled_time"];
      
      // Parse time (format: "HH:MM:SS")
      int hour = atoi(timeStr);
      int minute = atoi(timeStr + 3);
      
      // Store medication
      medications[medicationCount].id = id;
      strncpy(medications[medicationCount].name, name, sizeof(medications[medicationCount].name) - 1);
      medications[medicationCount].name[sizeof(medications[medicationCount].name) - 1] = '\0';
      medications[medicationCount].hour = hour;
      medications[medicationCount].minute = minute;
      medications[medicationCount].reminded = false;
      medications[medicationCount].advanceReminded = false;
      medications[medicationCount].notificationActive = false;
      
      Serial.printf("Medication Task: Added %s at %02d:%02d\n", name, hour, minute);
      medicationCount++;
    }
    
    Serial.printf("Medication Task: Loaded %d medications\n", medicationCount);
    http.end();
    return true;
  }
  
  Serial.printf("Medication Task: HTTP error %d\n", httpCode);
  http.end();
  return false;
}

/**
 * Save medications to SPIFFS flash storage
 */
void saveMedicationsToFlash() {
  File file = SPIFFS.open("/medications.json", FILE_WRITE);
  if(!file) {
    Serial.println("Medication Task: Failed to open file for writing");
    return;
  }
  
  DynamicJsonDocument doc(4096);
  JsonArray array = doc.to<JsonArray>();
  
  for(int i = 0; i < medicationCount; i++) {
    JsonObject item = array.createNestedObject();
    item["id"] = medications[i].id;
    item["name"] = medications[i].name;
    item["hour"] = medications[i].hour;
    item["minute"] = medications[i].minute;
  }
  
  if(serializeJson(doc, file) == 0) {
    Serial.println("Medication Task: Failed to write to file");
  }
  
  file.close();
}

/**
 * Load medications from SPIFFS flash storage
 */
void loadMedicationsFromFlash() {
  if(!SPIFFS.exists("/medications.json")) {
    Serial.println("Medication Task: No saved medications found");
    return;
  }
  
  File file = SPIFFS.open("/medications.json", FILE_READ);
  if(!file) {
    Serial.println("Medication Task: Failed to open file for reading");
    return;
  }
  
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if(error) {
    Serial.println("Medication Task: Failed to parse file");
    return;
  }
  
  medicationCount = 0;
  JsonArray array = doc.as<JsonArray>();
  
  for(JsonObject med : array) {
    if(medicationCount >= MAX_MEDS) break;
    
    medications[medicationCount].id = med["id"];
    strncpy(medications[medicationCount].name, med["name"], sizeof(medications[medicationCount].name) - 1);
    medications[medicationCount].name[sizeof(medications[medicationCount].name) - 1] = '\0';
    medications[medicationCount].hour = med["hour"];
    medications[medicationCount].minute = med["minute"];
    medications[medicationCount].reminded = false;
    medications[medicationCount].advanceReminded = false;
    medications[medicationCount].notificationActive = false;
    
    medicationCount++;
  }
  
  Serial.printf("Medication Task: Loaded %d medications from storage\n", medicationCount);
}

/**
 * Update upcoming medication information
 */
void updateUpcomingMedication() {
  if(medicationCount == 0) {
    hasUpcomingMed = false;
    return;
  }
  
  // Get current time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Medication Task: Failed to obtain time for upcoming calculation");
    return;
  }
  
  // Calculate current minutes since midnight
  int currentTotalMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  
  // Find the next upcoming medication
  int closestMedIndex = -1;
  int closestTimeDiff = 24 * 60; // Maximum minutes in a day
  
  for(int i = 0; i < medicationCount; i++) {
    int medTotalMinutes = medications[i].hour * 60 + medications[i].minute;
    
    // Calculate time difference, handling the case of tomorrow's medications
    int timeDiff = medTotalMinutes - currentTotalMinutes;
    if(timeDiff < 0) {
      timeDiff += 24 * 60; // Add a full day if medication time is earlier than current time
    }
    
    if(timeDiff < closestTimeDiff) {
      closestTimeDiff = timeDiff;
      closestMedIndex = i;
    }
  }
  
  if(closestMedIndex >= 0) {
    // Update upcoming medication info
    strncpy(upcomingMedName, medications[closestMedIndex].name, sizeof(upcomingMedName) - 1);
    upcomingMedName[sizeof(upcomingMedName) - 1] = '\0';
    upcomingMedHour = medications[closestMedIndex].hour;
    upcomingMedMinute = medications[closestMedIndex].minute;
    hasUpcomingMed = true;
    
    Serial.printf("Medication Task: Next upcoming medication is %s at %02d:%02d\n", 
                 upcomingMedName, upcomingMedHour, upcomingMedMinute);
    
    // Update shared data for display
    updateUpcomingMedicationDisplay();
  } else {
    hasUpcomingMed = false;
  }
}

/**
 * Update the upcoming medication information on the display
 */
void updateUpcomingMedicationDisplay() {
  if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    if(hasUpcomingMed) {
      // Format time with leading zeros
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", upcomingMedHour, upcomingMedMinute);
      
      // Update shared upcoming medication info
      strncpy((char*)upcomingMedication.name, upcomingMedName, sizeof(upcomingMedication.name) - 1);
      upcomingMedication.name[sizeof(upcomingMedication.name) - 1] = '\0';
      strncpy((char*)upcomingMedication.timeStr, timeStr, sizeof(upcomingMedication.timeStr) - 1);
      upcomingMedication.timeStr[sizeof(upcomingMedication.timeStr) - 1] = '\0';
      upcomingMedication.available = true;
    } else {
      upcomingMedication.available = false;
    }
    
    upcomingMedicationUpdated = true;
    xSemaphoreGive(displayMutex);
    
    // Signal other tasks that upcoming medication info is updated
    xSemaphoreGive(medicationSemaphore);
  }
}

/**
 * Check if it's time to take any medications
 */
void checkMedications() {
  // Get current time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Medication Task: Failed to obtain time");
    return;
  }
  
  // Reset reminded flags at midnight
  static int lastDay = -1;
  if(timeinfo.tm_mday != lastDay) {
    lastDay = timeinfo.tm_mday;
    for(int i = 0; i < medicationCount; i++) {
      medications[i].reminded = false;
      medications[i].advanceReminded = false;
    }
    
    // Also update upcoming medication at midnight
    updateUpcomingMedication();
  }
  
  // Check each medication
  for(int i = 0; i < medicationCount; i++) {
    // Check for 1-minute advance notification
    int advanceHour = medications[i].hour;
    int advanceMinute = medications[i].minute - 1;
    
    // Handle minute wrapping
    if(advanceMinute < 0) {
      advanceMinute = 59;
      advanceHour--;
      if(advanceHour < 0) advanceHour = 23;
    }
    
    // Check for advance reminder (1 minute before)
    if(!medications[i].advanceReminded && 
       timeinfo.tm_hour == advanceHour && 
       timeinfo.tm_min == advanceMinute) {
      
      // Trigger advance notification
      triggerMedicationAdvanceReminder(medications[i].name);
      medications[i].advanceReminded = true;
      medications[i].notificationActive = true;
      medications[i].notificationStartTime = millis();
    }
    
    // Check for main reminder (at scheduled time)
    if(!medications[i].reminded && 
       timeinfo.tm_hour == medications[i].hour && 
       timeinfo.tm_min == medications[i].minute) {
      
      // Trigger the main medication reminder
      triggerMedicationReminder(medications[i].name);
      medications[i].reminded = true;
      medications[i].notificationActive = true;
      medications[i].notificationStartTime = millis();
      
      // Update upcoming medication after one has been triggered
      updateUpcomingMedication();
    }
  }
}

/**
 * Play medication sound alert
 */
void playMedicationSound(const char* medicationName, bool isAdvance) {
  if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    currentAudioCommand.fileNumber = AUDIO_MEDICATION;
    currentAudioCommand.repeatCount = isAdvance ? 1 : 2; // Shorter for advance notification
    currentAudioCommand.volume = AUDIO_MAX_VOLUME;
    audioCommandUpdated = true;
    
    xSemaphoreGive(displayMutex);
    xSemaphoreGive(audioCommandSemaphore);
  }
}

/**
 * Trigger a 1-minute advance medication reminder
 */
void triggerMedicationAdvanceReminder(const char* medicationName) {
  // Update shared medication reminder data with mutex protection
  if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Copy medication name to global variable
    strncpy((char*)currentMedicationReminder.name, medicationName, sizeof(currentMedicationReminder.name) - 1);
    currentMedicationReminder.name[sizeof(currentMedicationReminder.name) - 1] = '\0';
    
    currentMedicationReminder.time = time(NULL);
    currentMedicationReminder.taken = false;
    currentMedicationReminder.isAdvanceNotice = true; // Flag this as an advance notice
    medicationReminderUpdated = true;
    
    xSemaphoreGive(displayMutex);
    
    // Signal other tasks
    xSemaphoreGive(medicationSemaphore);
    
    Serial.printf("Medication Task: ADVANCE Reminder for %s (1 minute before)\n", medicationName);
    
    // Play audio alert
    playMedicationSound(medicationName, true);
  }
}

/**
 * Trigger a medication reminder
 */
void triggerMedicationReminder(const char* medicationName) {
  // Update shared medication reminder data with mutex protection
  if(xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
    // Copy medication name to global variable
    strncpy((char*)currentMedicationReminder.name, medicationName, sizeof(currentMedicationReminder.name) - 1);
    currentMedicationReminder.name[sizeof(currentMedicationReminder.name) - 1] = '\0';
    
    currentMedicationReminder.time = time(NULL);
    currentMedicationReminder.taken = false;
    currentMedicationReminder.isAdvanceNotice = false; // This is the main notice
    medicationReminderUpdated = true;
    
    xSemaphoreGive(displayMutex);
    
    // Signal other tasks
    xSemaphoreGive(medicationSemaphore);
    
    
    Serial.printf("Medication Task: Reminder for %s (Telegram alert triggered)\n", medicationName);
    
    // Play audio alert
    playMedicationSound(medicationName, false);
  }
}
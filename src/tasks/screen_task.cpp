/**
 * ElderGuard - Screen Task Implementation
 * 
 * This file implements the OLED display task that shows heart rate, 
 * medication reminders, fall alerts, and other status information.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <time.h>
#include "../include/screen_task.h"
#include "../include/config.h"
#include "../include/globals.h"

// -------------------- Display --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SH110X_I2C_ADDRESS 0x3C
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- Time --------------------
char timeString[9]; // HH:MM:SS
char heartRateStr[10] = "78 BPM"; // Default value
String medicineName = "";

// -------------------- Icons --------------------
// Small heart icon (8x8)
const unsigned char PROGMEM heartIconSmall[] = {
    0x3C, 0x7E, 0xFF, 0xFF,
    0xFF, 0x7E, 0x3C, 0x18
};

// Larger heart icon (unused in this version)
const unsigned char PROGMEM heartIcon[] = {
    0x0C, 0x30, 0x1E, 0x78, 0x3F, 0xFC, 0x7F, 0xFE, 
    0x7F, 0xFE, 0x3F, 0xFC, 0x1F, 0xF8, 0x0F, 0xF0,
    0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80
};

// Pill icon for medicine (8x8)
const unsigned char PROGMEM pillIcon[] = {
    0x3C, 0x42, 0xA9, 0x85, 0x85, 0xA9, 0x42, 0x3C
};

// Location/GPS icon (8x8)
const unsigned char PROGMEM locationIcon[] = {
    0x18, 0x3C, 0x7E, 0xFF,
    0xFF, 0xFF, 0x66, 0x00
};

// Current values to display
int currentHeartRate = 0;
char currentMedicationName[32] = "";
bool medicationAlertActive = false;

// Flag to indicate if display needs immediate update
bool needsDisplayUpdate = false;

void screenTask(void *pvParameters) {
    Serial.println("Screen Task: Started");
    
    // Initialize I2C
    Wire.begin(21, 22);
    delay(100);
    
    // Display init
    Serial.println("Screen Task: Initializing display...");
    if (!display.begin(SH110X_I2C_ADDRESS, OLED_RESET)) {
        Serial.println("Display init failed");
        int retries = 0;
        while (!display.begin(SH110X_I2C_ADDRESS, OLED_RESET) && retries < 5) {
            Serial.println("Screen Task: Retrying display initialization...");
            retries++;
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        
        if (retries >= 5) {
            Serial.println("Screen Task: Display initialization failed after retries");
            // Continue anyway, don't block other tasks
        }
    } else {
        Serial.println("Display initialized successfully");
    }
    
    display.setRotation(0);
    display.clearDisplay();
    display.display();
    
    // Show welcome screen
    display.clearDisplay();
    
    // Use textSize 2 for a better fit
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    
    // Calculate centered position for "ElderGuard"
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("ElderGuard", 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    int y = (SCREEN_HEIGHT - h) / 2;
    
    display.setCursor(x, y);
    display.println("ElderGuard");
    display.display();
    delay(3000);
    
    // Variables for task timing
    unsigned long lastUpdateTime = 0;
    unsigned long lastHeartRateUpdateTime = 0;
    const unsigned long updateInterval = 1000; // Update display every 1 second
    
    // Main task loop
    while (true) {
        // Get current time
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            strcpy(timeString, "??:??:??");
        } else {
            strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
        }
        
        // Check for new ECG data using semaphore (non-blocking)
        if (xSemaphoreTake(ecgDataSemaphore, 0) == pdTRUE) {
            // ECG data was updated - access the shared data
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Always update heart rate display regardless of signal validity
                int prevHeartRate = currentHeartRate;
                currentHeartRate = currentEcgData.heartRate;
                
                // Log when heart rate changes
                if (prevHeartRate != currentHeartRate) {
                    Serial.printf("Screen Task: Heart rate updated from %d to %d (Signal: %s)\n", 
                                 prevHeartRate, currentHeartRate,
                                 currentEcgData.validSignal ? "Valid" : "Invalid");
                    lastHeartRateUpdateTime = millis();
                    needsDisplayUpdate = true;
                }
                
                // Force update every 2 seconds even if the heart rate hasn't changed
                // This ensures "NO SIGNAL" is displayed when sensors are disconnected
                unsigned long currentMillis = millis();
                if (currentMillis - lastHeartRateUpdateTime > 2000) {
                    lastHeartRateUpdateTime = currentMillis;
                    needsDisplayUpdate = true;
                    Serial.println("Screen Task: Forcing display update for heart rate");
                }
                
                xSemaphoreGive(displayMutex);
            }
        }
        
        // Check for medication reminders using semaphore
        if (xSemaphoreTake(medicationSemaphore, 0) == pdTRUE) {
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!currentMedicationReminder.taken) {
                    medicationAlertActive = true;
                    // Fix volatile char pointer issue with character-by-character copy
                    for (size_t i = 0; i < sizeof(currentMedicationName) - 1 && i < sizeof(currentMedicationReminder.name); i++) {
                        currentMedicationName[i] = currentMedicationReminder.name[i];
                        if (currentMedicationReminder.name[i] == '\0') break;
                    }
                    currentMedicationName[sizeof(currentMedicationName) - 1] = '\0'; // Ensure null termination
                } else {
                    medicationAlertActive = false;
                }
                needsDisplayUpdate = true;
                xSemaphoreGive(displayMutex);
            }
        }
        
        // Update display at regular intervals or when new data received
        unsigned long currentTime = millis();
        if (needsDisplayUpdate || currentTime - lastUpdateTime >= updateInterval) {
            lastUpdateTime = currentTime;
            
            // Take the display mutex
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Update display based on current state
                if (medicationAlertActive) {
                    displayMedicationReminder(currentMedicationName);
                } else {
                    displayMainScreen();
                }
                
                // Release the display mutex
                xSemaphoreGive(displayMutex);
                needsDisplayUpdate = false;
            }
        }
        
        // Debug: Check for stale heart rate data
        if (currentTime - lastHeartRateUpdateTime > 5000) {
            Serial.println("Screen Task: WARNING - No heart rate updates for 5+ seconds");
        }
        
        // Small delay to prevent task from hogging CPU
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// -------------------- Main Display --------------------
void displayMainScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    // Time - Top center
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timeString, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x, 2);
    display.print(timeString);

    // Horizontal line below time
    display.drawFastHLine(0, 12, SCREEN_WIDTH, SH110X_WHITE);

    // Heart Rate with heart icon - Left section
    display.drawBitmap(5, 18, heartIconSmall, 8, 8, SH110X_WHITE);
    display.setCursor(15, 18);
    sprintf(heartRateStr, "%d BPM", currentHeartRate);
    display.print(heartRateStr);

    // Draw heart health status indicator
    display.setCursor(5, 30);
    if (currentHeartRate > 0) {
        if (currentHeartRate < 60) {
            display.print("LOW RATE");
        } else if (currentHeartRate > 100) {
            display.print("HIGH RATE");
        } else {
            display.print("NORMAL");
        }
    } else {
        display.print("NO SIGNAL");
    }

    // Middle divider
    display.drawFastVLine(SCREEN_WIDTH/2 - 2, 14, 32, SH110X_WHITE);

    // Medicine with pill icon - Right section
    display.drawBitmap(SCREEN_WIDTH/2 + 5, 18, pillIcon, 8, 8, SH110X_WHITE);
    display.setCursor(SCREEN_WIDTH/2 + 15, 18);
    display.print("Medicine");
    
    display.setCursor(SCREEN_WIDTH/2 + 5, 30);
    if (strlen(currentMedicationName) > 0) {
        // Truncate long medication names
        char shortName[10] = ""; // Buffer for shortened name
        strncpy(shortName, currentMedicationName, 9);
        shortName[9] = '\0'; // Ensure null termination
        display.print(shortName);
    } else {
        display.print("None due");
    }

    // Horizontal line
    display.drawFastHLine(0, 46, SCREEN_WIDTH, SH110X_WHITE);

    // Status bar - Bottom
    display.setCursor(2, 52);
    display.print("GPS: ");
    // Check GPS status from globals
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        display.print(currentGpsData.validFix ? "Active" : "Searching");
        xSemaphoreGive(displayMutex);
    } else {
        display.print("Unknown");
    }

    // // Battery indicator placeholder
    // display.setCursor(90, 52);
    // display.print("Batt:OK");

    display.display();
}

void displayMedicationReminder(const char* medicationName) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    
    // Title
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.println("MEDICATION TIME");
    
    // Draw pill icon
    display.drawBitmap(60, 2, pillIcon, 8, 8, SH110X_WHITE);
    
    // Medicine name (larger text)
    display.setTextSize(2);
    // Center text if possible, otherwise just display
    if (strlen(medicationName) < 10) {
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(medicationName, 0, 0, &x1, &y1, &w, &h);
        int x = (SCREEN_WIDTH - w) / 2;
        display.setCursor(x, 25);
    } else {
        display.setCursor(2, 25);
    }
    display.println(medicationName);
    
    // Time
    display.setTextSize(1);
    display.setCursor(35, 50);
    display.print(timeString);
    
    display.display();
}
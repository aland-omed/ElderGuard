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

// Screen timeout definitions (milliseconds)
#define MEDICATION_SCREEN_TIMEOUT 17000  // 17 seconds
#define FALL_SCREEN_TIMEOUT 20000        // 20 seconds
#define MAIN_SCREEN_REFRESH 1000         // 1 second refresh

// Screen state tracking
enum DisplayScreenState {
    SCREEN_MAIN,
    SCREEN_MEDICATION,
    SCREEN_FALL
};

// Current display state
DisplayScreenState currentScreenState = SCREEN_MAIN;
unsigned long screenStateStartTime = 0;

// -------------------- Time --------------------
char timeString[9]; // HH:MM:SS
char heartRateStr[10] = "78 BPM"; // Default value
String medicineName = "";

// -------------------- Icons --------------------
// Small heart icon (8x8)
const unsigned char PROGMEM heartIconSmall[] = {
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

// WiFi signal strength icons (8x8)
const unsigned char PROGMEM wifiNone[] = {
    0x00, 0x00, 0x00, 0x1C,
    0x22, 0x22, 0x1C, 0x00
};

const unsigned char PROGMEM wifiWeak[] = {
    0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x18, 0x18
};

const unsigned char PROGMEM wifiMedium[] = {
    0x00, 0x00, 0x00, 0x00,
    0x3C, 0x3C, 0x18, 0x18
};

const unsigned char PROGMEM wifiStrong[] = {
    0x00, 0x00, 0x7E, 0x7E,
    0x3C, 0x3C, 0x18, 0x18
};

// Alert icon (8x8)
const unsigned char PROGMEM alertIcon[] = {
    0x18, 0x3C, 0x3C, 0x7E,
    0x7E, 0xFF, 0xFF, 0x18
};

// Current values to display
int currentHeartRate = 0;
char currentMedicationName[32] = "";
bool medicationAlertActive = false;
unsigned long lastHeartBeatAnimation = 0;
bool heartBeatState = false;

// Helper function to safely draw text centered on x-axis
void drawCenteredText(const char* text, int y, int size = 1) {
    display.setTextSize(size);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    display.setCursor(x, y);
    display.print(text);
}

// Function to safely draw WiFi signal strength icon
void drawWifiIcon(int rssi) {
    // RSSI values typically range from -100 (weak) to -30 (strong)
    int signalStrength;
    
    if (rssi < -85 || rssi == 0) {
        signalStrength = 0; // No/very weak signal
    } else if (rssi < -70) {
        signalStrength = 1; // Weak signal
    } else if (rssi < -55) {
        signalStrength = 2; // Medium signal
    } else {
        signalStrength = 3; // Strong signal
    }
    
    // Draw the appropriate icon
    switch (signalStrength) {
        case 0:
            display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiNone, 8, 8, SH110X_WHITE);
            break;
        case 1:
            display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiWeak, 8, 8, SH110X_WHITE);
            break;
        case 2:
            display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiMedium, 8, 8, SH110X_WHITE);
            break;
        case 3:
            display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiStrong, 8, 8, SH110X_WHITE);
            break;
    }
}

// -------------------- Main Display --------------------
void displayMainScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    // Time - Top center
    display.setTextSize(1);
    drawCenteredText(timeString, 2);
    
    // WiFi signal strength indicator - Top right
    if (wifiStatusUpdated) {
        drawWifiIcon(currentWiFiStatus.rssi);
    } else {
        // Default to no signal if WiFi status unknown
        display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiNone, 8, 8, SH110X_WHITE);
    }

    // Horizontal line below time
    display.drawFastHLine(0, 12, SCREEN_WIDTH, SH110X_WHITE);

    // Heart Rate with heart icon - Left section
    // Animate heart icon
    unsigned long currentMillis = millis();
    if (currentMillis - lastHeartBeatAnimation > 1000) {
        heartBeatState = !heartBeatState;
        lastHeartBeatAnimation = currentMillis;
    }
    
    if (heartBeatState) {
        display.drawBitmap(5, 18, heartIconSmall, 16, 11, SH110X_WHITE);
    } else {
        display.drawBitmap(5, 18, heartIconSmall, 16, 11, SH110X_WHITE);
    }

    // Heart rate value
    display.setCursor(23, 18);
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
    // Check if we have an upcoming medication
    if (upcomingMedicationUpdated && upcomingMedication.available) {
        // Truncate long medication names
        char shortName[10] = ""; // Buffer for shortened name
        strncpy(shortName, (const char*)upcomingMedication.name, 9);
        shortName[9] = '\0'; // Ensure null termination
        display.print(shortName);
        
        // Show scheduled time
        display.setCursor(SCREEN_WIDTH/2 + 5, 40);
        display.print((const char*)upcomingMedication.timeStr);
    } else if (medicationAlertActive && strlen(currentMedicationName) > 0) {
        // Truncate long medication names
        char shortName[10] = ""; // Buffer for shortened name
        strncpy(shortName, currentMedicationName, 9);
        shortName[9] = '\0'; // Ensure null termination
        display.print(shortName);
        
        // Flash "TAKE NOW" if medication is due
        if ((millis() / 500) % 2 == 0) {
            display.setCursor(SCREEN_WIDTH/2 + 5, 40);
            display.print("TAKE NOW");
        }
    } else {
        display.print("None due");
    }

    // Horizontal line
    display.drawFastHLine(0, 46, SCREEN_WIDTH, SH110X_WHITE);

    // Status bar - Bottom
    display.setCursor(2, 52);
    display.print("GPS: ");
    // Check GPS status
    if (gpsDataUpdated) {
        display.print(currentGpsData.validFix ? "Active" : "Search");
    } else {
        display.print("N/A");
    }

    // Display the GPS icon
    display.drawBitmap(65, 52, locationIcon, 8, 8, SH110X_WHITE);

    // Show fall alert if active
    if (fallDetectionUpdated && currentFallEvent.fallDetected) {
        if ((millis() / 500) % 2 == 0) {
            display.setCursor(75, 52);
            display.print("FALL!");
        }
    }

    display.display();
}

// -------------------- Medication Reminder Display --------------------
void displayMedicationReminder(const char* medicationName) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    
    // Create flashing border
    if ((millis() / 500) % 2 == 0) {
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
    }
    
    // Title
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.println("MEDICATION TIME");
    
    // Draw pill icon
    display.drawBitmap(110, 2, pillIcon, 8, 8, SH110X_WHITE);
    
    // Status bar separator
    display.drawFastHLine(0, 12, SCREEN_WIDTH, SH110X_WHITE);
    
    // Medicine name (larger text)
    display.setTextSize(2);
    
    // Center text if possible, otherwise just display
    if (strlen(medicationName) < 10) {
        drawCenteredText(medicationName, 25, 2);
    } else {
        // For longer names, use smaller text
        display.setTextSize(1);
        drawCenteredText(medicationName, 25);
    }
    
    // Current time
    display.setTextSize(1);
    drawCenteredText(timeString, 48);
    
    // Add visual confirmation button
    display.fillRoundRect((SCREEN_WIDTH - 90) / 2, 54, 90, 10, 3, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
    drawCenteredText("CONFIRM TAKEN", 55);
    display.setTextColor(SH110X_WHITE);
    
    display.display();
}

// -------------------- Fall Alert Display --------------------
void displayFallAlert() {
    display.clearDisplay();
    
    // Make the text prominent with large size and flashing
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    
    // Create flashing border effect
    if ((millis() / 250) % 2 == 0) {
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
        display.drawRect(2, 2, SCREEN_WIDTH-4, SCREEN_HEIGHT-4, SH110X_WHITE);
    }
    
    // Center and display the alert message
    drawCenteredText("FALL", 15, 2);
    drawCenteredText("DETECTED", 35, 2);
    
    // Display fall severity if available
    if (fallDetectionUpdated && currentFallEvent.fallDetected) {
        display.setTextSize(1);
        char severityStr[20];
        sprintf(severityStr, "Severity: %d/10", currentFallEvent.fallSeverity);
        drawCenteredText(severityStr, 55);
    }
    
    display.display();
}

// -------------------- Screen Task --------------------
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
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    drawCenteredText("ElderGuard", (SCREEN_HEIGHT - 16) / 2, 2);
    display.display();
    delay(2000);  // Shorter welcome screen to avoid task watchdog
    
    // Variables for task timing and state
    unsigned long lastUpdateTime = 0;
    unsigned long lastHeartRateUpdateTime = 0;
    const unsigned long updateInterval = 1000; // Update display every 1 second
    currentScreenState = SCREEN_MAIN;
    screenStateStartTime = millis();
    
    // Main task loop
    while (true) {
        unsigned long currentTime = millis();
        
        // Get current time
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo)) {
            strcpy(timeString, "??:??:??");
        } else {
            strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
        }
        
        // Check for screen timeouts to prevent screens from being stuck
        if (currentScreenState != SCREEN_MAIN) {
            unsigned long elapsedTime = currentTime - screenStateStartTime;
            
            if ((currentScreenState == SCREEN_MEDICATION && elapsedTime >= MEDICATION_SCREEN_TIMEOUT) ||
                (currentScreenState == SCREEN_FALL && elapsedTime >= FALL_SCREEN_TIMEOUT)) {
                
                currentScreenState = SCREEN_MAIN;
                needsDisplayUpdate = true;
                Serial.println("Screen Task: Alert timeout, returning to main screen");
            }
        }
        
        // SAFELY Check for fall detection using semaphore (non-blocking)
        bool hasFallUpdate = false;
        bool isFallDetected = false;
        
        if (xSemaphoreTake(fallDetectionSemaphore, 0) == pdTRUE) {
            hasFallUpdate = true;
            // Safely copy fall detection data while holding the semaphore
            if (fallDetectionUpdated) {
                isFallDetected = currentFallEvent.fallDetected;
            }
            xSemaphoreGive(fallDetectionSemaphore);
        }
        
        if (hasFallUpdate) {
            if (isFallDetected) {
                currentScreenState = SCREEN_FALL;
                screenStateStartTime = currentTime;
            } else {
                currentScreenState = SCREEN_MAIN;
            }
            needsDisplayUpdate = true;
        }
        
        // SAFELY Check for ECG data using semaphore (non-blocking)
        if (xSemaphoreTake(ecgDataSemaphore, 0) == pdTRUE) {
            // Safely copy ECG data while holding the semaphore
            int prevHeartRate = currentHeartRate;
            if (ecgDataUpdated) {
                currentHeartRate = currentEcgData.heartRate;
                
                if (prevHeartRate != currentHeartRate) {
                    lastHeartRateUpdateTime = currentTime;
                    needsDisplayUpdate = true;
                }
            }
            xSemaphoreGive(ecgDataSemaphore);
        }
        
        // Force heart rate display update even if no new data
        if (currentTime - lastHeartRateUpdateTime > 2000) {
            lastHeartRateUpdateTime = currentTime;
            needsDisplayUpdate = true;
        }
        
        // SAFELY Check for medication reminders using semaphore
        bool hasMedicationUpdate = false;
        bool isReminderActive = false;
        char medicationNameTemp[32] = "";
        
        if (xSemaphoreTake(medicationSemaphore, 0) == pdTRUE) {
            hasMedicationUpdate = true;
            
            // Safely copy medication data while holding the semaphore
            if (medicationReminderUpdated) {
                isReminderActive = !currentMedicationReminder.taken;
                
                // Safely copy the medication name
                if (isReminderActive) {
                    strncpy(medicationNameTemp, (const char*)currentMedicationReminder.name, sizeof(medicationNameTemp)-1);
                    medicationNameTemp[sizeof(medicationNameTemp)-1] = '\0'; // Ensure null termination
                }
            }
            
            xSemaphoreGive(medicationSemaphore);
            
            // Update state variables AFTER releasing the semaphore
            if (hasMedicationUpdate) {
                medicationAlertActive = isReminderActive;
                if (isReminderActive) {
                    strncpy(currentMedicationName, medicationNameTemp, sizeof(currentMedicationName));
                    
                    // Only switch to medication screen if it's newly active
                    currentScreenState = SCREEN_MEDICATION;
                    screenStateStartTime = currentTime;
                }
                needsDisplayUpdate = true;
            }
        }
        
        // Update display at regular intervals or when new data received
        if (needsDisplayUpdate || currentTime - lastUpdateTime >= updateInterval) {
            lastUpdateTime = currentTime;
            
            // No need to take displayMutex here since we're the only task that accesses the display directly
            
            // Update display based on current state
            switch (currentScreenState) {
                case SCREEN_FALL:
                    displayFallAlert();
                    break;
                case SCREEN_MEDICATION:
                    displayMedicationReminder(currentMedicationName);
                    break;
                default:
                    displayMainScreen();
                    break;
            }
            
            needsDisplayUpdate = false;
        }
        
        // Give other tasks a chance to run
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
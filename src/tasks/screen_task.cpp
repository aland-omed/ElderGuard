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
// Redesigned for better organization and clarity
void displayMainScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // --- Top Bar ---
    // Time centered
    drawCenteredText(timeString, 2);
    // WiFi Icon right
    if (wifiStatusUpdated) {
        drawWifiIcon(currentWiFiStatus.rssi);
    } else {
        // Default icon if status unknown
        display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiNone, 8, 8, SH110X_WHITE);
    }
    // Separator
    display.drawFastHLine(0, 12, SCREEN_WIDTH, SH110X_WHITE);

    // --- Heart Rate Area (Left Half) ---
    // Heart Icon Animation
    unsigned long currentMillis = millis();
    if (currentMillis - lastHeartBeatAnimation > 700) { // Slightly faster beat animation
        heartBeatState = !heartBeatState;
        lastHeartBeatAnimation = currentMillis;
    }
    if (heartBeatState) {
         display.drawBitmap(5, 18, heartIconSmall, 16, 11, SH110X_WHITE);
    } else {
         // Optionally clear the icon area or draw a static version if preferred
         // For now, just redraw it (subtle blink effect)
         display.drawBitmap(5, 18, heartIconSmall, 16, 11, SH110X_WHITE);
    }
    // Heart Rate Value (Larger Font)
    display.setTextSize(2);
    sprintf(heartRateStr, "%d", currentHeartRate > 0 ? currentHeartRate : 0); // Show 0 if no signal
    display.setCursor(25, 18); // Adjusted position
    display.print(heartRateStr);
    display.setTextSize(1);
    display.print(" BPM");
    // Heart Rate Status (Smaller Font)
    display.setCursor(5, 38); // Below value
    if (currentHeartRate > 0) {
        if (currentHeartRate < 60) display.print("LOW");
        else if (currentHeartRate > 100) display.print("HIGH");
        else display.print("NORMAL");
    } else {
        display.print("--"); // Indicate no signal clearly
    }

    // --- Vertical Divider ---
    display.drawFastVLine(SCREEN_WIDTH/2 - 1, 14, 34, SH110X_WHITE); // Adjusted height

    // --- Medication Area (Right Half) ---
    display.setTextSize(1);
    // Pill Icon
    display.drawBitmap(SCREEN_WIDTH/2 + 5, 18, pillIcon, 8, 8, SH110X_WHITE);
    // Label
    display.setCursor(SCREEN_WIDTH/2 + 15, 18);
    display.print("Next Med:");
    // Medication Info
    display.setCursor(SCREEN_WIDTH/2 + 5, 30); // Below label
    if (upcomingMedicationUpdated && upcomingMedication.available) {
        // Truncate name
        char shortName[10];
        strncpy(shortName, (const char*)upcomingMedication.name, 9);
        shortName[9] = '\0';
        display.print(shortName);
        // Time
        display.setCursor(SCREEN_WIDTH/2 + 5, 40); // Below name
        display.print((const char*)upcomingMedication.timeStr);
    } else if (medicationAlertActive && strlen(currentMedicationName) > 0) {
         // Truncate name
        char shortName[10];
        strncpy(shortName, currentMedicationName, 9);
        shortName[9] = '\0';
        display.print(shortName);
        // Flash "TAKE NOW"
        if ((millis() / 500) % 2 == 0) {
            display.setCursor(SCREEN_WIDTH/2 + 5, 40); // Below name
            display.print("TAKE NOW");
        } else {
             // Clear the area when not flashing
             display.fillRect(SCREEN_WIDTH/2 + 5, 40, SCREEN_WIDTH/2 - 5, 8, SH110X_BLACK);
        }
    }
     else {
        display.print("None due");
    }

    // --- Bottom Separator ---
    display.drawFastHLine(0, 50, SCREEN_WIDTH, SH110X_WHITE); // Adjusted y-coord

    // --- Status Bar ---
    // GPS Status
    display.drawBitmap(5, 53, locationIcon, 8, 8, SH110X_WHITE); // Adjusted coords
    display.setCursor(15, 54); // Adjusted coords
    display.print("GPS:");
    if (gpsDataUpdated) {
        display.print(currentGpsData.validFix ? "Fix" : "Search");
    } else {
        display.print("N/A");
    }

    // Fall Status (Right side of status bar)
    if (fallDetectionUpdated && currentFallEvent.fallDetected) {
        // Flash "FALL!" with icon
        if ((millis() / 500) % 2 == 0) {
            display.drawBitmap(SCREEN_WIDTH - 45, 53, alertIcon, 8, 8, SH110X_WHITE); // Adjusted coords
            display.setCursor(SCREEN_WIDTH - 35, 54); // Adjusted coords
            display.print("FALL!");
        } else {
             // Clear the area when not flashing
             display.fillRect(SCREEN_WIDTH - 45, 53, 45, 10, SH110X_BLACK);
        }
    }
    // No "OK" status needed, keep it clean

    display.display();
}

// -------------------- Medication Reminder Display --------------------
// Redesigned for clarity and urgency
void displayMedicationReminder(const char* medicationName) {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);

    // --- Flashing Border ---
    bool flashState = (millis() / 500) % 2 == 0;
    if (flashState) {
        display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
        display.drawRect(1, 1, SCREEN_WIDTH-2, SCREEN_HEIGHT-2, SH110X_WHITE); // Thicker border
    }
    // No inverse needed, just flashing border

    // --- Title ---
    display.setTextSize(2); // Larger title
    drawCenteredText("MEDICINE", 5, 2);

    // --- Pill Icon --- (Centered below title)
    display.drawBitmap((SCREEN_WIDTH - 8) / 2, 22, pillIcon, 8, 8, SH110X_WHITE);

    // --- Medicine Name ---
    display.setTextSize(1); // Use smaller text for potentially long names
    char displayName[20]; // Allow slightly longer display
    strncpy(displayName, medicationName, 19);
    displayName[19] = '\0';
    drawCenteredText(displayName, 35); // Centered below icon

    // --- Instruction ---
    display.setTextSize(2);
    // Flash "TAKE NOW" text
    if (flashState) {
        drawCenteredText("TAKE NOW", 50, 2); // Large instruction
    } else {
        // Clear the text area when not flashing
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds("TAKE NOW", 0, 0, &x1, &y1, &w, &h);
        display.fillRect((SCREEN_WIDTH - w) / 2, 50, w, h, SH110X_BLACK);
    }

    // Removed the non-functional "Confirm Taken" button visual

    display.display();
}

// -------------------- Fall Alert Display --------------------
// Redesigned for maximum visibility
void displayFallAlert() {
    display.clearDisplay();

    // --- Flashing Inverse Background/Text ---
    bool flashState = (millis() / 300) % 2 == 0; // Faster flash rate
    display.fillScreen(flashState ? SH110X_WHITE : SH110X_BLACK);
    display.setTextColor(flashState ? SH110X_BLACK : SH110X_WHITE);

    // --- Alert Icon --- (Centered at top)
    display.drawBitmap((SCREEN_WIDTH - 8) / 2, 5, alertIcon, 8, 8, flashState ? SH110X_BLACK : SH110X_WHITE);

    // --- Main Message ---
    display.setTextSize(2);
    drawCenteredText("FALL", 20, 2); // Adjusted position
    drawCenteredText("DETECTED", 40, 2); // Adjusted position

    // --- Severity ---
    if (fallDetectionUpdated && currentFallEvent.fallDetected && currentFallEvent.fallSeverity > 0) {
        display.setTextSize(1);
        char severityStr[20];
        sprintf(severityStr, "Severity: %d", currentFallEvent.fallSeverity); // Simplified text
        // Position severity at the bottom
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(severityStr, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((SCREEN_WIDTH - w) / 2, SCREEN_HEIGHT - h - 2); // Bottom center
        display.print(severityStr);
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
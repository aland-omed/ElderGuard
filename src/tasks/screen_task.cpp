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

// External queue handles declared in main.cpp
extern QueueHandle_t ecgDataQueue;
extern QueueHandle_t gpsDataQueue;
extern QueueHandle_t fallDetectionQueue;
extern QueueHandle_t medicationQueue;
extern SemaphoreHandle_t displayMutex;

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
bool fallAlertActive = false;
bool gpsFixValid = false;

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
    showWelcomeScreen();
    
    // Variables for task timing
    unsigned long lastUpdateTime = 0;
    const unsigned long updateInterval = 1000; // Update display every 1 second
    
    // Main task loop
    while (true) {
        // Get current time
        updateTime();
        
        // Check queues for new data
        checkEcgQueue();
        checkGpsQueue();
        checkFallQueue();
        checkMedicationQueue();
        
        // Update display at regular intervals
        unsigned long currentTime = millis();
        if (currentTime - lastUpdateTime >= updateInterval) {
            lastUpdateTime = currentTime;
            
            // Take the display mutex
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Update display based on current state
                if (fallAlertActive) {
                    displayFallAlert();
                } else if (medicationAlertActive) {
                    displayMedicationReminder(currentMedicationName);
                } else {
                    displayMainScreen();
                }
                
                // Release the display mutex
                xSemaphoreGive(displayMutex);
            }
        }
        
        // Small delay to prevent task from hogging CPU
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// -------------------- Welcome --------------------
void showWelcomeScreen() {
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
}

// -------------------- Main Display --------------------
void displayMainScreen() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);

    // Time - Top left
    display.setTextSize(1);
    display.setCursor(2, 2);
    display.print(timeString);

    // Medicine with pill icon - Center left
    // Draw pill icon
    display.drawBitmap(5, 30, pillIcon, 8, 8, SH110X_WHITE);
    display.setCursor(15, 32);
    display.print("Medicine: ");
    display.println(strlen(currentMedicationName) > 0 ? currentMedicationName : "None");

    // Heart Rate with small heart icon - Bottom left
    display.drawBitmap(3, 53, heartIconSmall, 8, 8, SH110X_WHITE);
    display.setCursor(13, 55);
    sprintf(heartRateStr, "%d BPM", currentHeartRate);
    display.print(heartRateStr);

    // GPS status - Top right
    if (gpsFixValid) {
        display.drawBitmap(SCREEN_WIDTH - 10, 2, locationIcon, 8, 8, SH110X_WHITE);
    } else {
        display.drawRect(SCREEN_WIDTH - 10, 2, 8, 8, SH110X_WHITE);
    }

    display.display();
}

void displayFallAlert() {
    display.clearDisplay();
    display.setTextColor(SH110X_WHITE);
    
    // Large text for fall alert
    display.setTextSize(2);
    
    // Center text
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("FALL", 0, 0, &x1, &y1, &w, &h);
    int x = (SCREEN_WIDTH - w) / 2;
    
    display.setCursor(x, 5);
    display.println("FALL");
    
    display.getTextBounds("DETECTED", 0, 0, &x1, &y1, &w, &h);
    x = (SCREEN_WIDTH - w) / 2;
    
    display.setCursor(x, 25);
    display.println("DETECTED");
    
    // Flash the display
    static bool flashState = false;
    flashState = !flashState;
    
    if (flashState) {
        // Invert display for flashing effect
        display.invertDisplay(true);
    } else {
        display.invertDisplay(false);
    }
    
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

// -------------------- Update Time --------------------
void updateTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        strcpy(timeString, "??:??:??");
        return;
    }
    strftime(timeString, sizeof(timeString), "%H:%M:%S", &timeinfo);
}

void checkEcgQueue() {
    EcgData ecgData;
    

    if (xQueueReceive(ecgDataQueue, &ecgData, 0) == pdTRUE) {
        if (ecgData.validSignal) {
          
            currentHeartRate = ecgData.heartRate;
        }
    }
}

void checkGpsQueue() {
    GpsData gpsData;
    if (xQueueReceive(gpsDataQueue, &gpsData, 0) == pdTRUE) {
        gpsFixValid = gpsData.validFix;
    }
}

void checkFallQueue() {
    FallEvent fallEvent;
    if (xQueueReceive(fallDetectionQueue, &fallEvent, 0) == pdTRUE) {
        if (fallEvent.fallDetected) {
            fallAlertActive = true;
        }
    }
}

void checkMedicationQueue() {
    MedicationReminder medReminder;
    if (xQueueReceive(medicationQueue, &medReminder, 0) == pdTRUE) {
        if (!medReminder.taken) {
            medicationAlertActive = true;
            strncpy(currentMedicationName, medReminder.name, sizeof(currentMedicationName) - 1);
            currentMedicationName[sizeof(currentMedicationName) - 1] = '\0'; // Ensure null termination
        } else {
            medicationAlertActive = false;
        }
    }
}
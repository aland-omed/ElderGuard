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

// WiFi signal strength icons (8x8) - Improved designs
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

// Current values to display
int currentHeartRate = 0;
char currentMedicationName[32] = "";
bool medicationAlertActive = false;

// Function to draw WiFi signal strength icon
void drawWifiIcon(int rssi) {
    // RSSI values typically range from -100 (weak) to -30 (strong)
    // Convert to a 0-3 scale for icon selection
    int signalStrength;
    
    if (rssi < -85) {
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
        
        // Check for fall detection using semaphore (non-blocking)
        if (xSemaphoreTake(fallDetectionSemaphore, 0) == pdTRUE) {
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Check if this is a fall event or a reset
                if (currentFallEvent.fallDetected) {
                    Serial.println("Screen Task: Fall detected! Updating display...");
                    displayFallAlert();
                } else {
                    Serial.println("Screen Task: Fall event reset after 40 seconds. Returning to main screen.");
                    displayMainScreen();
                }
                xSemaphoreGive(displayMutex);
                needsDisplayUpdate = false; // Already handled the update
                
                // Wait a bit to ensure the alert is visible
                vTaskDelay(pdMS_TO_TICKS(100));
            }
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
                if (fallDetectionUpdated && currentFallEvent.fallDetected) {
                    displayFallAlert();
                } else if (medicationAlertActive) {
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
    // Check GPS status directly - we already have the mutex when this function is called
    display.print(currentGpsData.validFix ? "Active" : "Searching");

    // Display the GPS icon
    display.drawBitmap(65, 52, locationIcon, 8, 8, SH110X_WHITE);

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
    
    // WiFi signal strength indicator - Top right
    if (wifiStatusUpdated) {
        drawWifiIcon(currentWiFiStatus.rssi);
    } else {
        // Default to no signal if WiFi status unknown
        display.drawBitmap(SCREEN_WIDTH - 12, 2, wifiNone, 8, 8, SH110X_WHITE);
    }
    
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

// Add fall alert screen display function
void displayFallAlert() {
  display.clearDisplay();
  
  // Make the text prominent with large size and flashing
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  
  // Center and display the alert message
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds("FALL", 0, 0, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 15);
  display.println("FALL");
  
  display.getTextBounds("DETECTED", 0, 0, &x1, &y1, &w, &h);
  x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, 40);
  display.println("DETECTED");
  
  // Display fall severity if available
  if (fallDetectionUpdated && currentFallEvent.fallDetected) {
    display.setTextSize(1);
    char severityStr[20];
    sprintf(severityStr, "Severity: %d/10", currentFallEvent.fallSeverity);
    display.setCursor(25, 60);
    display.print(severityStr);
  }
  
  display.display();
}
/**
 * ElderGuard - Screen Task
 * 
 * This file declares the screen task interface for the OLED display.
 */

#ifndef SCREEN_TASK_H
#define SCREEN_TASK_H

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <time.h>
#include "config.h"

// Function prototypes
void screenTask(void *pvParameters);
void displayMainScreen();
void displayFallAlert();
void displayMedicationReminder(const char* medicationName);
void showWelcomeScreen();
void updateTime();
void connectToWiFi();
void checkEcgQueue();
void checkGpsQueue();
void checkFallQueue();
void checkMedicationQueue();

#endif // SCREEN_TASK_H
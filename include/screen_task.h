/**
 * ElderGuard - Screen Display Task
 * 
 * This file contains the headers for the screen display task
 * that manages the OLED display for the ElderGuard system.
 */

#ifndef SCREEN_TASK_H
#define SCREEN_TASK_H

#include <Arduino.h>
#include "config.h"
#include "globals.h"

// ECG Buffer size (needs to match the size in ecg_task.cpp)
#define ECG_BUFFER_SIZE 250

// Function prototypes
void screenTask(void *pvParameters);
void displayMainScreen();
void displayMedicationReminder(const char* medicationName);
void displayEcgWaveform(int* buffer, int bufferSize, int bufferIndex);
void displayFallAlert();

#endif // SCREEN_TASK_H
/**
 * ElderGuard - Time Management Task Header
 * 
 * This file contains declarations for time synchronization management in the ElderGuard system.
 */

#ifndef TIME_TASK_H
#define TIME_TASK_H

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include "config.h"

/**
 * Main time management task function
 * This task handles NTP time synchronization and provides current time
 * 
 * @param pvParameters Task parameters (not used)
 */
void timeTask(void *pvParameters);

/**
 * Initialize time synchronization with NTP servers
 * 
 * @param status Pointer to time status structure
 * @return true if initialization successful, false otherwise
 */
bool setupTimeSync(volatile TimeStatus *status);

/**
 * Synchronize system time with NTP servers
 * 
 * @param status Pointer to time status structure
 * @return true if sync successful, false otherwise
 */
bool syncTimeWithNTP(volatile TimeStatus *status);

/**
 * Update current time information in the status structure
 * 
 * @param status Pointer to time status structure
 */
void updateCurrentTime(volatile TimeStatus *status);

/**
 * Get current time as an epoch timestamp
 * 
 * @return Current time as unix timestamp (seconds since epoch)
 */
time_t getCurrentEpochTime();

/**
 * Get current time as a formatted string
 * 
 * @param buffer Buffer to store formatted time string
 * @param bufferSize Size of the buffer
 * @param format Format string (strftime format)
 * @return Pointer to the buffer
 */
char* getCurrentTimeString(char* buffer, size_t bufferSize, const char* format);

/**
 * Check if the time has been synchronized
 * 
 * @return true if time is synchronized, false otherwise
 */
bool isTimeSynchronized();

#endif // TIME_TASK_H
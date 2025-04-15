/**
 * ElderGuard - Audio Task
 * 
 * This file declares the audio task interface for sound alerts
 * using the MP3-TF-16P module.
 */

#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include <Arduino.h>
#include "config.h"

// Function prototypes
void audioTask(void *pvParameters);
void playAudioFile(int fileNumber, int repeatCount);
bool initializeMP3Player();

#endif // AUDIO_TASK_H
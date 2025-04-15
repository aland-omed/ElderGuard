/**
 * ElderGuard - Audio Task
 * 
 * This file contains the declarations for the audio notification task
 * that manages MP3 audio alerts for the ElderGuard system.
 */

#ifndef AUDIO_TASK_H
#define AUDIO_TASK_H

#include <Arduino.h>
#include "config.h"
#include "globals.h"

// Function prototypes
void audioTask(void *pvParameters);
bool initializeMP3Player();
void playAudioFile(int fileNumber, int repeatCount);
void stopAudio();

#endif // AUDIO_TASK_H
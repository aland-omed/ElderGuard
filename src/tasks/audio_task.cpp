/**
 * ElderGuard - Audio Task Implementation
 * 
 * This file implements the audio alert task using the MP3-TF-16P module
 * to play sounds for different events like fall detection and medication reminders.
 */

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <DFRobotDFPlayerMini.h>
#include "../include/audio_task.h"
#include "../include/config.h"
#include "../include/globals.h"

// DFPlayer Mini object
DFRobotDFPlayerMini mp3Player;
bool mp3PlayerAvailable = false;

void audioTask(void *pvParameters) {
  Serial.println("Audio Task: Started");
  
  // Initialize MP3 player
  mp3PlayerAvailable = initializeMP3Player();
  
  if (mp3PlayerAvailable) {
    // Make sure volume is set before playing anything
    mp3Player.volume(AUDIO_MAX_VOLUME);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Play welcome sound explicitly
    Serial.println("Audio Task: Playing welcome sound");
    mp3Player.play(AUDIO_WELCOME);
    vTaskDelay(pdMS_TO_TICKS(3000)); // Allow welcome message to play
  } else {
    Serial.println("Audio Task: MP3 player not available");
  }
  
  // Main task loop
  while (true) {
    // Check for audio commands using semaphore
    if (xSemaphoreTake(audioCommandSemaphore, pdMS_TO_TICKS(500)) == pdTRUE) {
      // Audio command was signaled - access the shared data
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Get audio command data - create a local copy to avoid volatile issues
        AudioCommand audioCmd;
        audioCmd.fileNumber = currentAudioCommand.fileNumber;
        audioCmd.repeatCount = currentAudioCommand.repeatCount;
        audioCmd.volume = currentAudioCommand.volume;
        
        bool audioUpdated = audioCommandUpdated;
        audioCommandUpdated = false; // Reset the flag
        
        // Release mutex
        xSemaphoreGive(displayMutex);
        
        // Process the audio command
        if (audioUpdated && mp3PlayerAvailable) {
          Serial.printf("Audio Task: Playing file #%d for %d times\n", 
                     audioCmd.fileNumber, audioCmd.repeatCount);
          
          // Always use maximum volume
          mp3Player.volume(AUDIO_MAX_VOLUME);
          
          // Play the sound
          playAudioFile(audioCmd.fileNumber, audioCmd.repeatCount);
        } else if (audioUpdated) {
          // Fallback if MP3 player not available - just print to serial
          Serial.print("Audio Task: Would play sound file ");
          Serial.print(audioCmd.fileNumber);
          Serial.print(" for ");
          Serial.print(audioCmd.repeatCount);
          Serial.println(" times");
        }
      }
    }
    
    // Small delay to prevent task from hogging CPU
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

bool initializeMP3Player() {
  // Initialize MP3-TF-16P UART connection
  Serial1.begin(9600, SERIAL_8N1, MP3_RX_PIN, MP3_TX_PIN);
  vTaskDelay(pdMS_TO_TICKS(1500)); // Allow module time to boot
  
  // Clear any leftover data from the serial buffer
  while (Serial1.available()) {
    Serial1.read();
  }
  
  // Try initializing the module up to 3 times
  for (int attempts = 0; attempts < 3; attempts++) {
    Serial.printf("Audio Task: MP3-TF-16P init attempt %d...\n", attempts + 1);
    if (mp3Player.begin(Serial1, true)) { // 'true' for debug info
      Serial.println("Audio Task: MP3-TF-16P initialized successfully!");
      
      // Configure module settings
      mp3Player.setTimeOut(1000);
      mp3Player.volume(AUDIO_MAX_VOLUME); // Maximum volume level
      mp3Player.EQ(DFPLAYER_EQ_NORMAL);
      mp3Player.outputDevice(DFPLAYER_DEVICE_SD);
      vTaskDelay(pdMS_TO_TICKS(200));
      
      uint8_t vol = mp3Player.readVolume();
      if (vol == 255) {
        Serial.println("Audio Task: MP3-TF-16P communication error detected.");
      } else {
        Serial.printf("Audio Task: MP3-TF-16P volume confirmed: %d\n", vol);
        return true;
      }
    }
    
    Serial.println("Audio Task: MP3-TF-16P initialization failed, retrying...");
    Serial1.flush();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  Serial.println("Audio Task: WARNING: MP3-TF-16P not available!");
  return false;
}

void playAudioFile(int fileNumber, int repeatCount) {
  if (!mp3PlayerAvailable) {
    return;
  }
  
  Serial.printf("Audio Task: Playing file #%d for %d times\n", fileNumber, repeatCount);
  
  // Optional: Check if module is responsive
  uint8_t state = mp3Player.readState();
  if (state == 255) {
    Serial.println("Audio Task: MP3-TF-16P not responding, attempting reset...");
    mp3Player.reset();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  
  // Play the sound repeatedly if needed
  for (int i = 0; i < repeatCount; i++) {
    mp3Player.play(fileNumber);
    
    // Wait for the audio to complete (typical MP3 is 2-3 seconds)
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    if (repeatCount > 1 && i < repeatCount - 1) {
      // Small gap between repetitions
      vTaskDelay(pdMS_TO_TICKS(500));
    }
  }
}
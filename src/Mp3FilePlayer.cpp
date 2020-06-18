// Simple MP3 player example
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// This example code is in the public domain.

#include <Wire.h>
#include <SdFat.h>
#include <sdios.h>

#include "my_play_sd_mp3.h"
#include "my_output_i2s.h"

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3

#if SD_FAT_TYPE == 3
SdFs sd;
FsFile file;
#else  // SD_FAT_TYPE
#error Invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// SDCARD_SS_PIN is defined for the built-in SD on some boards.
#ifndef SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SS;
#else  // SDCARD_SS_PIN
const uint8_t SD_CS_PIN = SDCARD_SS_PIN;
#endif  // SDCARD_SS_PIN

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(16))
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(16))
#endif  // HAS_SDIO_CLASS

// GUItool: begin automatically generated code
MyAudioPlaySdMp3         playMp3;
MyAudioOutputI2S         i2s1;
AudioConnection          patchCord0(playMp3, 0, i2s1, 0);
AudioConnection          patchCord1(playMp3, 1, i2s1, 1);

void setup() {
  Serial.begin(9600);

  // Initialize the SD.
  if (!sd.begin(SD_CONFIG)) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  Serial.println("SD card OK");

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(5);
}

void playFile(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playMp3.play(filename);

  // Simply wait for the file to finish playing.
  while (playMp3.isPlaying()) {
    // uncomment these lines if your audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    // sgtl5000_1.volume(vol);

#if 0	
	 Serial.print("Max Usage: ");
	 Serial.print(playMp3.processorUsageMax());
	 Serial.print("% Audio, ");
	 Serial.print(playMp3.processorUsageMaxDecoder());
	 Serial.print("% Decoding max, ");
	 
	 Serial.print(playMp3.processorUsageMaxSD());
	 Serial.print("% SD max, ");
	 	 
	 Serial.print(AudioProcessorUsageMax());
	 Serial.println("% All");
	 
	 AudioProcessorUsageMaxReset();
	 playMp3.processorUsageMaxReset();
	 playMp3.processorUsageMaxResetDecoder();
#endif 
	 
	 delay(250);
  }
}


void loop() {
  playFile("Tom.mp3");
  playFile("02 - Rock 'n' Roll Damnation.mp3");
  playFile("03 - Guns for Hire.mp3");
  delay(500);
}

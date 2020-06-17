// Simple MP3 player example
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// This example code is in the public domain.

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <play_sd_mp3.h>
#include "my_play_sd_mp3.h"
#include "my_output_i2s.h"

// GUItool: begin automatically generated code
MyAudioPlaySdMp3         playMp3;
MyAudioOutputI2S         i2s1;
AudioConnection          patchCord0(playMp3, 0, i2s1, 0);
AudioConnection          patchCord1(playMp3, 1, i2s1, 1);

void setup() {
  Serial.begin(9600);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(20);

  //SPI.setMOSI(7);
  //SPI.setSCK(14);
  if (!(SD.begin(BUILTIN_SDCARD))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
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

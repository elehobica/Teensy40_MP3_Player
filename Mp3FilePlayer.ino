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
#include "my_output_i2s.h"


// GUItool: begin automatically generated code
AudioPlaySdMp3           playMp31;       //xy=154,78
MyAudioOutputI2S           i2s1;           //xy=334,89
AudioMixer4              mix0;
AudioMixer4              mix1;
AudioConnection          patchCord0(playMp31, 0, mix0, 0);
AudioConnection          patchCord1(playMp31, 1, mix1, 0);
AudioConnection          patchCord2(mix0, 0, i2s1, 0);
AudioConnection          patchCord3(mix1, 0, i2s1, 1);
// GUItool: end automatically generated code

void setup() {
  Serial.begin(9600);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(5);

  //SPI.setMOSI(7);
  //SPI.setSCK(14);
  if (!(SD.begin(BUILTIN_SDCARD))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  mix0.gain(0, 1.0);
  mix1.gain(0, 1.0);
}

void playFile(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playMp31.play(filename);

  // Simply wait for the file to finish playing.
  while (playMp31.isPlaying()) {
    // uncomment these lines if your audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    // sgtl5000_1.volume(vol);

#if 0	
	 Serial.print("Max Usage: ");
	 Serial.print(playMp31.processorUsageMax());
	 Serial.print("% Audio, ");
	 Serial.print(playMp31.processorUsageMaxDecoder());	 	 
	 Serial.print("% Decoding max, ");
	 
	 Serial.print(playMp31.processorUsageMaxSD());	 
	 Serial.print("% SD max, ");
	 	 
	 Serial.print(AudioProcessorUsageMax());	 
	 Serial.println("% All");
	 
	 AudioProcessorUsageMaxReset();
	 playMp31.processorUsageMaxReset();
	 playMp31.processorUsageMaxResetDecoder();
#endif 
	 
	 delay(250);
  }
}


void loop() {
  playFile("ForTag.mp3");	
  playFile("Tom.mp3");
  playFile("Foreverm.mp3");
  delay(500);
}

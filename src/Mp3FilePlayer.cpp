// Simple MP3 player example
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// This example code is in the public domain.

#include <Wire.h>
#include <SdFat.h>

#include "my_play_sd_mp3.h"
#include "my_output_i2s.h"

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3

#if SD_FAT_TYPE == 2
SdExFat sd;
ExFile dir;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile dir;
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

#define NUM_BTN_HISTORY   30
#define HP_BUTTON_OPEN    0
#define HP_BUTTON_CENTER  1
#define HP_BUTTON_D       2
#define HP_BUTTON_PLUS    3
#define HP_BUTTON_MINUS   4
uint8_t button_prv[NUM_BTN_HISTORY] = {}; // initialized as HP_BUTTON_OPEN

IntervalTimer myTimer;

MyAudioPlaySdMp3         playMp3;
MyAudioOutputI2S         i2s1;
AudioConnection          patchCord0(playMp3, 0, i2s1, 0);
AudioConnection          patchCord1(playMp3, 1, i2s1, 1);

uint8_t adc0_get_hp_button(void)
{
    uint8_t ret;
    const int max = 1023;
    uint16_t adc0_rdata = analogRead(PIN_A8);
    // 3.3V support
    if (adc0_rdata < max*100/3300) { // < 100mV  4095*100/3300 (CENTER)
        ret = HP_BUTTON_CENTER;
    } else if (adc0_rdata >= max*142/3300 && adc0_rdata < max*238/3300) { // 142mv ~ 238mV (D: 190mV)
        ret = HP_BUTTON_D;
    } else if (adc0_rdata >= max*240/3300 && adc0_rdata < max*400/3300) { // 240mV ~ 400mV (PLUS: 320mV)
        ret = HP_BUTTON_PLUS;
    } else if (adc0_rdata >= max*435/3300 && adc0_rdata < max*725/3300) { // 435mV ~ 725mV (MINUS: 580mV)
        ret = HP_BUTTON_MINUS;
    } else { // others
        ret = HP_BUTTON_OPEN;
    }
    return ret;
}

int count_center_clicks(void)
{
    int i;
    int detected_fall = 0;
    int count = 0;
    for (i = 0; i < 4; i++) {
        if (button_prv[i] != HP_BUTTON_OPEN) {
            return 0;
        }
    }
    for (i = 4; i < NUM_BTN_HISTORY; i++) {
        if (detected_fall == 0 && button_prv[i-1] == HP_BUTTON_OPEN && button_prv[i] == HP_BUTTON_CENTER) {
            detected_fall = 1;
        } else if (detected_fall == 1 && button_prv[i-1] == HP_BUTTON_CENTER && button_prv[i] == HP_BUTTON_OPEN) {
            count++;
            detected_fall = 0;
        }
    }
    if (count > 0) {
        for (i = 0; i < NUM_BTN_HISTORY; i++) button_prv[i] = HP_BUTTON_OPEN;
    }
    return count;
}

void tick_100ms(void)
{
  uint8_t button = adc0_get_hp_button();
  if (button == HP_BUTTON_PLUS) {
    i2s1.volume_up();
  } else if (button == HP_BUTTON_MINUS) {
    i2s1.volume_down();
  }
}

void setup() {
  Serial.begin(115200);
  myTimer.begin(tick_100ms, 100000);

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
  playFile("ForTag.mp3");
  playFile("01 - Shoot to Thrill.mp3");
  playFile("Tom.mp3");
  playFile("02 - Rock 'n' Roll Damnation.mp3");
  playFile("Foreverm.mp3");
  playFile("03 - Guns for Hire.mp3");
  delay(500);
}

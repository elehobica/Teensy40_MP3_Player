// Simple MP3 player example
//
// Requires the audio shield:
//   http://www.pjrc.com/store/teensy3_audio.html
//
// This example code is in the public domain.

#include <string.h>
#include <Wire.h>
#include <SdFat.h>

#include "LcdCanvas.h"
#include "my_play_sd_mp3.h"
#include "my_output_i2s.h"
#include "ff_util.h"
#include "stack.h"

FsBaseFile file;

#define NUM_BTN_HISTORY   30
#define HP_BUTTON_OPEN    0
#define HP_BUTTON_CENTER  1
#define HP_BUTTON_D       2
#define HP_BUTTON_PLUS    3
#define HP_BUTTON_MINUS   4
uint8_t button_prv[NUM_BTN_HISTORY] = {}; // initialized as HP_BUTTON_OPEN
uint32_t button_repeat_count = 0;

IntervalTimer myTimer;

// LCD (ST7735, 1.8", 128x160pix)
#define TFT_CS        10
#define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         8
//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
LcdCanvas lcd = LcdCanvas(TFT_CS, TFT_DC, TFT_RST);

MyAudioPlaySdMp3         playMp3;
MyAudioOutputI2S         i2s1;
AudioConnection          patchCord0(playMp3, 0, i2s1, 0);
AudioConnection          patchCord1(playMp3, 1, i2s1, 1);

stack_t *stack; // for change directory history
stack_data_t item;
int stack_count;

// FileView Menu
mode_enm mode = FileView;

#define NUM_IDX_ITEMS         10
int idx_req = 1;
int idx_req_open = 0;
int aud_req = 0;
bool aud_pause_flg = false;

uint16_t idx_head = 0;
uint16_t idx_column = 0;
uint16_t idx_play_count = 0;
uint16_t idx_idle_count = 0;
uint16_t idx_play;

uint8_t adc0_get_hp_button(void)
{
    uint8_t ret;
    const int max = 1023;
    uint16_t adc0_rdata = analogRead(PIN_A8);
    // 3.3V support
    if (adc0_rdata < max*100/3300) { // < 100mV (CENTER)
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

void idx_open(void)
{
    if (idx_req_open == 1) { return; }
    idx_req_open = 1;
}

void idx_close(void)
{
    if (idx_req_open == 1) { return; }
    idx_column = 0;
    idx_head = 0;
    idx_req_open = 1;
}

void idx_random_open(void)
{
    if (idx_req_open == 2) { return; }
    idx_req_open = 2;
}

void idx_inc(void)
{
    if (idx_req == 1) { return; }
    if (idx_head >= file_menu_get_size() - NUM_IDX_ITEMS && idx_column == NUM_IDX_ITEMS-1) { return; }
    if (idx_head + idx_column + 1 >= file_menu_get_size()) { return; }
    idx_req = 1;
    idx_column++;
    if (idx_column >= NUM_IDX_ITEMS) {
        if (idx_head + NUM_IDX_ITEMS >= file_menu_get_size() - NUM_IDX_ITEMS) {
            idx_column = NUM_IDX_ITEMS-1;
            idx_head++;
        } else {
            idx_column = 0;
            idx_head += NUM_IDX_ITEMS;
        }
    }
}

void idx_dec(void)
{
    if (idx_req == 1) { return; }
    if (idx_head == 0 && idx_column == 0) { return; }
    idx_req = 1;
    if (idx_column == 0) {
        if (idx_head - NUM_IDX_ITEMS < 0) {
            idx_column = 0;
            idx_head--;
        } else {
            idx_column = NUM_IDX_ITEMS-1;
            idx_head -= NUM_IDX_ITEMS;
        }
    } else {
        idx_column--;
    }
}

void idx_fast_inc(void)
{
    if (idx_req == 1) { return; }
    if (idx_head >= file_menu_get_size() - NUM_IDX_ITEMS && idx_column == NUM_IDX_ITEMS-1) { return; }
    if (idx_head + idx_column + 1 >= file_menu_get_size()) return;
    if (idx_head + NUM_IDX_ITEMS >= file_menu_get_size() - NUM_IDX_ITEMS) {
        idx_head = file_menu_get_size() - NUM_IDX_ITEMS;
        idx_inc();
    } else {
        idx_head += NUM_IDX_ITEMS;
    }
    idx_req = 1;
}

void idx_fast_dec(void)
{
    if (idx_req == 1) { return; }
    if (idx_head == 0 && idx_column == 0) { return; }
    if (idx_head - NUM_IDX_ITEMS < 0) {
        idx_head = 0;
        idx_dec();
    } else {
        idx_head -= NUM_IDX_ITEMS;
    }
    idx_req = 1;
}

void aud_pause(void)
{
    if (aud_req != 0) { return; }
    aud_req = 1;
}

void aud_stop(void)
{
    if (aud_req != 0) { return; }
    aud_req = 2;
}

void volume_up(void)
{
    i2s1.volume_up();
}

void volume_down(void)
{
    i2s1.volume_down();
}

void tick_100ms(void)
{
    int i;
    int center_clicks;
    uint8_t button = adc0_get_hp_button();
    if (button == HP_BUTTON_OPEN) {
        button_repeat_count = 0;
        center_clicks = count_center_clicks(); // must be called once per tick because button_prv[] status has changed
        if (center_clicks > 0) {
            {
                char _str[64];
                sprintf(_str, "center_clicks = %d", center_clicks);
                Serial.println(_str);
            }
            if (mode == FileView) {
                if (center_clicks == 1) {
                    idx_open();
                } else if (center_clicks == 2) {
                    idx_close();
                } else if (center_clicks == 3) {
                    //idx_random_open();
                }
            } else if (mode == Play) {
                if (center_clicks == 1) {
                    aud_pause();
                } else if (center_clicks == 2) {
                    aud_stop();
                }
            }
        }
    } else if (button_prv[0] == HP_BUTTON_OPEN) { // push
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == FileView) {
                idx_dec();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_inc();
            } else if (mode == Play) {
                volume_down();
            }
        }
    }  else if (button_repeat_count == 10) { // long push
        if (button == HP_BUTTON_CENTER) {
            button_repeat_count++; // only once and step to longer push event
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == FileView) {
                idx_fast_dec();
            } else if (mode == Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == FileView) {
                idx_fast_inc();
            } else if (mode == Play) {
                volume_down();
            }
        }
    } else if (button == button_prv[0]) {
        button_repeat_count++;
    }
    // Button status shift
    for (i = NUM_BTN_HISTORY-1; i >= 0; i--) {
        button_prv[i+1] = button_prv[i];
    }
    button_prv[0] = button;
}

void setup() {
    Serial.begin(115200);
    myTimer.begin(tick_100ms, 100000);

    stack = stack_init();
    file_menu_open_root_dir();

    // Audio connections require memory to work.  For more
    // detailed information, see the MemoryAndCpuUsage example
    AudioMemory(5);
}

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

void loop() {
    int i;
    char str[256];
    if (aud_req == 1) {
        playMp3.pause(aud_pause_flg);
        aud_pause_flg = !aud_pause_flg;
        aud_req = 0;
    } else if (aud_req == 2) {
        playMp3.stop();
        mode = FileView;
        lcd.switchToFileView();
        aud_req = 0;
        idx_req = 1;
    } else if (idx_req_open == 1) {
        if (file_menu_is_dir(idx_head+idx_column) > 0) { // Directory
            if (idx_head+idx_column > 0) { // normal directory
                item.head = idx_head;
                item.column = idx_column;
                stack_push(stack, &item);
            }
            if (idx_head+idx_column == 0) { // upper ("..") dirctory
                if (stack_get_count(stack) > 0) {
                    file_menu_ch_dir(idx_head+idx_column);
                } else { // Already in top directory
                    file_menu_close_dir();
                    file_menu_open_root_dir(); // Root directory
                    item.head = 0;
                    item.column = 0;
                    stack_push(stack, &item);
                }
                stack_pop(stack, &item);
                idx_head = item.head;
                idx_column = item.column;
            } else { // normal directory
                file_menu_ch_dir(idx_head+idx_column);
                idx_head = 0;
                idx_column = 0;
            }
            idx_req = 1;
        } else { // File
            file_menu_full_sort();
            idx_play = idx_head + idx_column;
            file_menu_get_fname(idx_play, str, sizeof(str));
            char* ext_pos = strrchr(str, '.');
            if (ext_pos) {
                if (strncmp(ext_pos, ".mp3", 4) == 0) {
                    mode = Play;
                    Serial.println(str);
                    file_menu_get_obj(idx_play, &file);
                    playMp3.play(&file);
                    idx_play_count = 0;
                    idx_idle_count = 0;
                    aud_pause_flg = false;
                    lcd.switchToPlay();
                }
            }
      /*
          file_menu_full_sort();

          // DAC MUTE_B Pin (0: Mute, 1: Normal) (PB6)
          rcu_periph_clock_enable(RCU_GPIOB);
          gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
          PB_OUT(6, 1);

          // After audio_init(), Never call file_menu_xxx() functions!
          // Otherwise, it causes conflict between main and int routine
          audio_init();
          mode = Play;
          // Load cover art
          fr = f_open(&fil, "cover.bin", FA_READ);
          if (fr == FR_OK) {
              image0 = (unsigned char *) malloc(80*70*2);
              image1 = (unsigned char *) malloc(80*10*2);
              if (image0 != NULL && image1 != NULL) {
                  fr = f_read(&fil, image0, 80*70*2, &br);
                  fr = f_read(&fil, image1, 80*10*2, &br);
                  cover_exists = 1;
              } else {
                  LEDR(0);
                  cover_exists = 0;
              }
              f_close(&fil);
          } else {
              image0 = NULL;
              image1 = NULL;
              cover_exists = 0;
              sprintf("_str, open error: cover.bin %d!, (int)fr);
              Serial.println(_str);
          }
          LCD_Clear(BLACK);
          BACK_COLOR=BLACK;
          if (idx_play > 1) {
              audio_play(idx_play - 1);
          } else {
              audio_play(idx_head + idx_column);
          }
          idx_play_count = 0;
          idx_idle_count = 0;
          */
        }
        idx_req_open = 0;
    } else if (idx_req_open == 2) { // Random Play
  /*
      if (!audio_is_playing_or_pausing()) {
          audio_stop();
      }
      stack_count = stack_get_count(stack);
      if (stack_count > 0) { // Random Play for same level folder
          for (i = 0; i < stack_count; i++) { // chdir to parent directory
              if (fs.fs_type == FS_EXFAT) { // This is for FatFs known bug for ".." in EXFAT
                  file_menu_close_dir();
                  file_menu_open_root_dir(); // Root directory
              } else {
                  file_menu_ch_dir(0); // ".."
              }
              stack_pop(stack, &item);
          }
          for (i = 0; i < stack_count; i++) { // chdir to child directory at random
              idx_head = (rand() % (file_menu_get_size() - 1)) + 1;
              idx_column = 0;
              file_menu_sort_entry(idx_head + idx_column, idx_head + idx_column + 1);
              while (file_menu_is_dir(idx_head+idx_column) <= 0) { // not directory
                  idx_head = (idx_head < file_menu_get_size() - 1) ? idx_head + 1 : 1;
                  file_menu_sort_entry(idx_head + idx_column, idx_head + idx_column + 1);
              }
              sprintf(_str, "[random_play] dir level: %d, idx: %d, name: %s\n\r", i, idx_head + idx_column, file_menu_get_fname_ptr(idx_head + idx_column));
              Serial.println(_str);
              file_menu_ch_dir(idx_head + idx_column);
              item.head = idx_head;
              item.column = idx_column;
              stack_push(stack, &item);
          }
          idx_head = 1;
          idx_column = 0;
          idx_req_open = 1;
      }
      */
    } else if (idx_req) {
        for (i = 0; i < NUM_IDX_ITEMS; i++) {
            if (idx_head+i >= file_menu_get_size()) {
                lcd.setFileItem(i, ""); // delete
                continue;
            }
            file_menu_get_fname(idx_head+i, str, sizeof(str));
            lcd.setFileItem(i, str, file_menu_is_dir(idx_head+i), (i == idx_column));
        }
        idx_req = 0;
        idx_idle_count = 0;
    } else {
        if (mode == Play) {
            if (!playMp3.isPlaying()) {
                while (++idx_play < file_menu_get_size()) { // Play next file
                    file_menu_get_fname(idx_play, str, sizeof(str));
                    char* ext_pos = strrchr(str, '.');
                    if (!ext_pos) { continue; }
                    if (strncmp(ext_pos, ".mp3", 4) == 0) {
                        Serial.println(str);
                        file_menu_get_obj(idx_play, &file);
                        playMp3.play(&file);
                        break;
                    }
              }
              if (!playMp3.isPlaying()) {
                  mode = FileView;
                  idx_req = 1;
                  idx_idle_count = 0;
              }
            }
            lcd.setVolume(i2s1.get_volume());
            lcd.setBitRate(playMp3.bitRate());
            lcd.setPlayTime(playMp3.positionMillis()/1000, playMp3.lengthMillis()/1000);
        } else {// if (mode == FileView)
            idx_idle_count++;
            if (idx_idle_count > 100) {
                file_menu_idle();
            }
        }
    }
    lcd.draw();
    delay(100);
}
// MP3 File Player
//
// Based on following sample code
//   http://www.pjrc.com/store/teensy3_audio.html
//
// This example code is in the public domain.

#include <Wire.h>
#include <EEPROM.h>
#include <LcdCanvas.h>
#include <ff_util.h>
#include <utf_conv.h>
#include <TeensyThreads.h>

#include <Audio.h>
#include <play_sd_mp3.h>
#include <play_sd_wav.h>
#include <play_sd_aac.h>
#include <play_sd_flac.h>
#include <output_i2s.h>

#include "stack.h"
#include "TagRead.h"

const int Version = 100;

const int LoopCycleMs = 50; // loop cycle (ms)
const int OneSec = 1000 / LoopCycleMs; // 1 Sec
const int OneMin = 60 * OneSec; // 1 Min

const int BackLightBoostCycles = 20 * OneSec;
const int WaitCyclesForRandomPlay = 1 * OneMin;
const int WaitCyclesForPowerOffWhenPaused = 3 * OneMin;

IntervalTimer myTimer;
volatile uint32_t tick_100ms_count = 0;

volatile uint16_t battery_x1000 = 4200; // battery voltage x 1000 (4.2V = 4200)

Threads::Event codec_event;

MutexFsBaseFile file;
size_t fpos = 0;
uint32_t samples_played = 0;

#define NUM_BTN_HISTORY   30
#define HP_BUTTON_OPEN    0
#define HP_BUTTON_CENTER  1
#define HP_BUTTON_D       2
#define HP_BUTTON_PLUS    3
#define HP_BUTTON_MINUS   4
uint8_t button_prv[NUM_BTN_HISTORY] = {}; // initialized as HP_BUTTON_OPEN
volatile uint32_t button_repeat_count = 0;

//#define BATTERY_VOLTAGE_MSG

//#define EEPROM_INITIALIZE
#define EEPROM_SIZE 1080
#define EEPROM_BASE 0
// Config Space (Byte Unit Access)
#define CFG_BASE    EEPROM_BASE
#define CFG_SIZE    48 // 0x30
#define CFG_EPRW_COUNT_L    (EEPROM_BASE + 0)
#define CFG_EPRW_COUNT_H    (EEPROM_BASE + 1)
#define CFG_SEED0           (EEPROM_BASE + 2)
#define CFG_SEED1           (EEPROM_BASE + 3)
#define CFG_VOLUME          (EEPROM_BASE + 4)
#define CFG_STACK_COUNT     (EEPROM_BASE + 5)
#define CFG_STACK_HEAD0_L   (EEPROM_BASE + 6)
#define CFG_STACK_HEAD0_H   (EEPROM_BASE + 7)
#define CFG_STACK_COLUMN0_L (EEPROM_BASE + 8)
#define CFG_STACK_COLUMN0_H (EEPROM_BASE + 9)
#define CFG_STACK_HEAD1_L   (EEPROM_BASE + 10)
#define CFG_STACK_HEAD1_H   (EEPROM_BASE + 11)
#define CFG_STACK_COLUMN1_L (EEPROM_BASE + 12)
#define CFG_STACK_COLUMN1_H (EEPROM_BASE + 13)
#define CFG_STACK_HEAD2_L   (EEPROM_BASE + 14)
#define CFG_STACK_HEAD2_H   (EEPROM_BASE + 15)
#define CFG_STACK_COLUMN2_L (EEPROM_BASE + 16)
#define CFG_STACK_COLUMN2_H (EEPROM_BASE + 17)
#define CFG_STACK_HEAD3_L   (EEPROM_BASE + 18)
#define CFG_STACK_HEAD3_H   (EEPROM_BASE + 19)
#define CFG_STACK_COLUMN3_L (EEPROM_BASE + 20)
#define CFG_STACK_COLUMN3_H (EEPROM_BASE + 21)
#define CFG_STACK_HEAD4_L   (EEPROM_BASE + 22)
#define CFG_STACK_HEAD4_H   (EEPROM_BASE + 23)
#define CFG_STACK_COLUMN4_L (EEPROM_BASE + 24)
#define CFG_STACK_COLUMN4_H (EEPROM_BASE + 25)
#define CFG_IDX_HEAD_L      (EEPROM_BASE + 26)
#define CFG_IDX_HEAD_H      (EEPROM_BASE + 27)
#define CFG_IDX_COLUMN_L    (EEPROM_BASE + 28)
#define CFG_IDX_COLUMN_H    (EEPROM_BASE + 29)
#define CFG_MODE            (EEPROM_BASE + 30)
#define CFG_IDX_PLAY_L      (EEPROM_BASE + 31)
#define CFG_IDX_PLAY_H      (EEPROM_BASE + 32)
#define CFG_PLAY_POS0       (EEPROM_BASE + 33)
#define CFG_PLAY_POS1       (EEPROM_BASE + 34)
#define CFG_PLAY_POS2       (EEPROM_BASE + 35)
#define CFG_PLAY_POS3       (EEPROM_BASE + 36)
#define CFG_PLAY_POS4       (EEPROM_BASE + 37)
#define CFG_PLAY_POS5       (EEPROM_BASE + 38)
#define CFG_PLAY_POS6       (EEPROM_BASE + 39)
#define CFG_PLAY_POS7       (EEPROM_BASE + 40)
#define CFG_SAMPLES_PLAYED0 (EEPROM_BASE + 41)
#define CFG_SAMPLES_PLAYED1 (EEPROM_BASE + 42)
#define CFG_SAMPLES_PLAYED2 (EEPROM_BASE + 43)
#define CFG_SAMPLES_PLAYED3 (EEPROM_BASE + 44)

#define PIN_BACKLIGHT_CONTROL   (15)
#define PIN_DCDC_SHDN_B         (16)
#define PIN_BATTERY_CHECK       (18)

uint16_t eprw_count; // EEPROM Write Count (to check for write endurance of 100,000 cycles)

#ifdef USE_ST7735_128x160
// LCD (ST7735, 1.8", 128x160pix)
#define TFT_CS        10
#define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         8
#define BACKLIGHT_HIGH 256 // n/256 PWM
#define BACKLIGHT_LOW  128 // n/256 PWM
#define NUM_IDX_ITEMS         10
#endif
#ifdef USE_ST7789_240x240_WOCS
// LCD (ST7789, 1.3", 240x240pix without CS)
#define TFT_CS        -1
#define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         8
#define BACKLIGHT_HIGH 256 // n/256 PWM
#define BACKLIGHT_LOW  128 // n/256 PWM
#define NUM_IDX_ITEMS         15
#endif
#ifdef USE_ILI9341_240x320
// LCD (ILI9341, 2.2", 240x320pix)
#define TFT_CS        10
#define TFT_RST       -1 // Connected to VCC
#define TFT_DC         8
#define BACKLIGHT_HIGH 64 // n/256 PWM
#define BACKLIGHT_LOW  32 // n/256 PWM
#define NUM_IDX_ITEMS         20
#endif

LcdCanvas lcd = LcdCanvas(TFT_CS, TFT_DC, TFT_RST);

AudioPlaySdMp3      playMp3;
AudioPlaySdWav      playWav;
AudioPlaySdAac      playAac;
AudioPlaySdFlac     playFlac;
AudioCodec          *codec = &playMp3;
AudioOutputI2S      i2s1;
AudioMixer4         mixer0;
AudioMixer4         mixer1;
AudioConnection     patchCordIn0_0(playMp3, 0, mixer0, 0);
AudioConnection     patchCordIn0_1(playMp3, 1, mixer1, 0);
AudioConnection     patchCordIn1_0(playWav, 0, mixer0, 1);
AudioConnection     patchCordIn1_1(playWav, 1, mixer1, 1);
AudioConnection     patchCordIn2_0(playAac, 0, mixer0, 2);
AudioConnection     patchCordIn2_1(playAac, 1, mixer1, 2);
AudioConnection     patchCordIn3_0(playFlac, 0, mixer0, 3);
AudioConnection     patchCordIn3_1(playFlac, 1, mixer1, 3);
AudioConnection     patchCordOut0(mixer0, 0, i2s1, 0);
AudioConnection     patchCordOut1(mixer1, 0, i2s1, 1);

stack_t *stack; // for change directory history
stack_data_t item;
int stack_count;

// FileView Menu
volatile LcdCanvas::mode_enm mode = LcdCanvas::FileView;
volatile LcdCanvas::mode_enm mode_prv = LcdCanvas::FileView;

volatile int idx_req = 1;
volatile int idx_req_open = 0;
volatile int aud_req = 0;

volatile uint16_t idx_head = 0;
volatile uint16_t idx_column = 0;
uint16_t idle_count = 0;
uint16_t idx_play = 0;
volatile bool is_waiting_next_random = false;

TagRead tag;

void initEEPROM(void)
{
    char str[64];
    eprw_count = ((uint16_t) EEPROM.read(CFG_EPRW_COUNT_H) << 8) | ((uint16_t) EEPROM.read(CFG_EPRW_COUNT_L));
    #ifdef EEPROM_INITIALIZE
    if (1) {
    #else
    if (eprw_count == 0xffff) { // Default Value Clear if area is not initialized
    #endif // EEPROM_INITIALIZE
        eprw_count = 0;
        // Clear the value other than zero
        EEPROM.write(CFG_VOLUME, 65);
        // Zero Clear
        for (int i = CFG_EPRW_COUNT_L; i < CFG_EPRW_COUNT_L + CFG_SIZE; i++) {
            if (i == CFG_VOLUME) { continue; }
            EEPROM.write(i, 0);
        }
    } else {
        for (int i = 0; i < CFG_SIZE; i++) {
            int value = EEPROM.read(CFG_BASE + i);
            if (i % 0x10 == 0) {
                sprintf(str, "CFG[0x%02x]: ", i);
                Serial.print(str);
            }
            sprintf(str, "%02x ", value);
            Serial.print(str);
            if (i % 0x10 == 0xf) {
                Serial.println();
            }
        }
    }
    sprintf(str, "EEPROM Write Count: %d", (int) eprw_count);
    Serial.println(str);
}

void loadFromEEPROM(void)
{
    bool err_flg = false;
    randomSeed(((uint16_t) EEPROM.read(CFG_SEED1) << 8) | ((uint16_t) EEPROM.read(CFG_SEED0)));
    i2s1.set_volume(EEPROM.read(CFG_VOLUME));
    // Resume last folder & play
    for (int i = EEPROM.read(CFG_STACK_COUNT) - 1; i >= 0; i--) {
        item.head = ((uint16_t) EEPROM.read(CFG_STACK_HEAD0_H + i*4) << 8) | ((uint16_t) EEPROM.read(CFG_STACK_HEAD0_L + i*4));
        item.column = ((uint16_t) EEPROM.read(CFG_STACK_COLUMN0_H + i*4) << 8) | ((uint16_t) EEPROM.read(CFG_STACK_COLUMN0_L + i*4));
        if (item.head+item.column >= file_menu_get_num()) { err_flg = true; break; } // idx overflow
        file_menu_sort_entry(item.head+item.column, item.head+item.column + 1);
        if (file_menu_is_dir(item.head+item.column) <= 0 || item.head+item.column == 0) { err_flg = true; break; } // Not Directory or Parent Directory
        stack_push(stack, &item);
        file_menu_ch_dir(item.head+item.column);
    }
    idx_head = ((uint16_t) EEPROM.read(CFG_IDX_HEAD_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_HEAD_L));
    idx_column = ((uint16_t) EEPROM.read(CFG_IDX_COLUMN_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_COLUMN_L));
    mode = static_cast<LcdCanvas::mode_enm>(EEPROM.read(CFG_MODE));
    if (idx_head+idx_column >= file_menu_get_num()) { err_flg = true; } // idx overflow
    if (err_flg) { // Load Error
        stack_delete(stack);
        stack = stack_init();
        file_menu_open_root_dir();
        idx_head = idx_column = 0;
        mode = LcdCanvas::FileView;
    }
    idx_play = ((uint16_t) EEPROM.read(CFG_IDX_PLAY_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_PLAY_L));
    if (mode == LcdCanvas::Play) {
        mode = LcdCanvas::FileView;
        idx_req_open = 1;
        uint64_t play_pos = 0;
        for (int i = 0; i < 8; i++) {
            play_pos |=  ((uint64_t) EEPROM.read(CFG_PLAY_POS0+i) << i*8);
        }
        fpos = (size_t) play_pos;
        for (int i = 0; i < 4; i++) {
            samples_played |=  ((uint32_t) EEPROM.read(CFG_SAMPLES_PLAYED0+i) << i*8);
        }
        /*{ // DEBUG
            char str[64];
            sprintf(str, "play_pos: %d, samples_played: %d", (int) play_pos, (int) samples_played);
            Serial.println(str);
        }*/
    }
}

void power_off(const char *msg = NULL)
{
    lcd.switchToPowerOff(msg);
    uint8_t volume = i2s1.get_volume();
    fpos = 0;
    samples_played = 0;
    if (codec->isPlaying()) {
        fpos = codec->fposition();
        samples_played = codec->getSamplesPlayed();
        while (i2s1.get_volume() > 0) {
            i2s1.volume_down();
            delay(5);
            yield(); // Arduino msg loop
        }
        codec->stop();
    }
    // Save Config Data to EEPROM
    eprw_count++;
    EEPROM.write(CFG_EPRW_COUNT_L, (uint8_t) (eprw_count & 0xff));
    EEPROM.write(CFG_EPRW_COUNT_H, (uint8_t) ((eprw_count >> 8) & 0xff));
    EEPROM.write(CFG_SEED0, (uint8_t) (millis() & 0xff));
    EEPROM.write(CFG_SEED1, (uint8_t) ((millis() >> 8) & 0xff));
    EEPROM.write(CFG_VOLUME, volume);
    EEPROM.write(CFG_STACK_COUNT, stack_get_count(stack));
    for (int i = 0; i < EEPROM.read(CFG_STACK_COUNT); i++) {
        stack_pop(stack, &item);
        EEPROM.write(CFG_STACK_HEAD0_L + i*4, (uint8_t) (item.head & 0xff));
        EEPROM.write(CFG_STACK_HEAD0_H + i*4, (uint8_t) ((item.head >> 8) & 0xff));
        EEPROM.write(CFG_STACK_COLUMN0_L + i*4, (uint8_t) (item.column & 0xff));
        EEPROM.write(CFG_STACK_COLUMN0_H + i*4, (uint8_t) ((item.column >> 8) & 0xff));
    }
    EEPROM.write(CFG_IDX_HEAD_L, (uint8_t) (idx_head & 0xff));
    EEPROM.write(CFG_IDX_HEAD_H, (uint8_t) ((idx_head >> 8) & 0xff));
    EEPROM.write(CFG_IDX_COLUMN_L, (uint8_t) (idx_column & 0xff));
    EEPROM.write(CFG_IDX_COLUMN_H, (uint8_t) ((idx_column >> 8) & 0xff));
    EEPROM.write(CFG_MODE, static_cast<uint8_t>(mode_prv));
    EEPROM.write(CFG_IDX_PLAY_L, (uint8_t) (idx_play & 0xff));
    EEPROM.write(CFG_IDX_PLAY_H, (uint8_t) ((idx_play >> 8) & 0xff));
    uint64_t play_pos = (uint64_t) fpos;
    for (int i = 0; i < 8; i++) {
        EEPROM.write(CFG_PLAY_POS0 + i, (uint8_t) ((play_pos >> i*8) & 0xff));
    }
    for (int i = 0; i < 4; i++) {
        EEPROM.write(CFG_SAMPLES_PLAYED0 + i, (uint8_t) ((samples_played >> i*8) & 0xff));
    }
    lcd.draw();
    delay(500);
    // Self Power Off
    /* do pin control here */
    // Endless Loop
    while (1) {
        delay(100);
        yield(); // Arduino msg loop
        digitalWrite(PIN_DCDC_SHDN_B, LOW);
    }
}

uint8_t adc0_get_hp_button(void)
{
    uint8_t ret;
    const int max = (1<<10) - 1;
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
    is_waiting_next_random = false;
}

void idx_close(void)
{
    if (idx_req_open == 1) { return; }
    idx_column = 0;
    idx_head = 0;
    idx_req_open = 1;
    is_waiting_next_random = false;
}

void idx_random_open(void)
{
    if (idx_req_open == 2) { return; }
    idx_req_open = 2;
    is_waiting_next_random = false;
}

void idx_inc(void)
{
    if (idx_req == 1) { return; }
    if (idx_head >= file_menu_get_num() - NUM_IDX_ITEMS && idx_column == NUM_IDX_ITEMS-1) { return; }
    if (idx_head + idx_column + 1 >= file_menu_get_num()) { return; }
    idx_req = 1;
    idx_column++;
    if (idx_column >= NUM_IDX_ITEMS) {
        if (idx_head + NUM_IDX_ITEMS >= file_menu_get_num() - NUM_IDX_ITEMS) {
            idx_column = NUM_IDX_ITEMS-1;
            idx_head++;
        } else {
            idx_column = 0;
            idx_head += NUM_IDX_ITEMS;
        }
    }
    is_waiting_next_random = false;
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
    is_waiting_next_random = false;
}

void idx_fast_inc(void)
{
    if (idx_req == 1) { return; }
    if (idx_head >= file_menu_get_num() - NUM_IDX_ITEMS && idx_column == NUM_IDX_ITEMS-1) { return; }
    if (idx_head + idx_column + 1 >= file_menu_get_num()) return;
    if (idx_head + NUM_IDX_ITEMS >= file_menu_get_num() - NUM_IDX_ITEMS) {
        idx_head = file_menu_get_num() - NUM_IDX_ITEMS;
        idx_inc();
    } else {
        idx_head += NUM_IDX_ITEMS;
    }
    idx_req = 1;
    is_waiting_next_random = false;
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
    is_waiting_next_random = false;
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
    idle_count = 0;
}

void volume_down(void)
{
    i2s1.volume_down();
    idle_count = 0;
}

void tick_100ms(void)
{
    if (millis() < 5 * 1000) { return; } // no reaction within 5 sec after boot
    if ((tick_100ms_count % (10*5)) == 0) { // Check Battery Voltage at 5 sec each
        digitalWrite(PIN_BATTERY_CHECK, HIGH);
        delayMicroseconds(100); // waiting for voltage stable
        uint32_t adc0_rdata = analogRead(PIN_A0);
        battery_x1000 = adc0_rdata * 3300 * (33+10) / 1023 / 33; // voltage divider: 1 Kohm + 3.3 Kohm, ADC ref: 3300mV
        #ifdef BATTERY_VOLTAGE_MSG
        Serial.print("Battery Voltage = ");
        Serial.println(battery_x1000);
        #endif // BATTERY_VOLTAGE_MSG
        digitalWrite(PIN_BATTERY_CHECK, LOW);
    }
    __disable_irq();
    int i;
    int center_clicks;
    uint8_t button = adc0_get_hp_button();
    if (button == HP_BUTTON_OPEN) {
        button_repeat_count = 0;
        center_clicks = count_center_clicks(); // must be called once per tick because button_prv[] status has changed
        if (center_clicks > 0) {
            {
                char str[64];
                sprintf(str, "center_clicks = %d", center_clicks);
                Serial.println(str);
            }
            if (mode == LcdCanvas::FileView) {
                if (center_clicks == 1) {
                    idx_open();
                } else if (center_clicks == 2) {
                    idx_close();
                } else if (center_clicks == 3) {
                    idx_random_open();
                }
            } else if (mode == LcdCanvas::Play) {
                if (center_clicks == 1) {
                    aud_pause();
                } else if (center_clicks == 2) {
                    aud_stop();
                } else if (center_clicks == 3) {
                    idx_random_open();
                }
            }
        }
    } else if (button_prv[0] == HP_BUTTON_OPEN) { // push
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == LcdCanvas::FileView) {
                idx_dec();
            } else if (mode == LcdCanvas::Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == LcdCanvas::FileView) {
                idx_inc();
            } else if (mode == LcdCanvas::Play) {
                volume_down();
            }
        }
    } else if (button_repeat_count == 10) { // long push
        if (button == HP_BUTTON_CENTER) {
            button_repeat_count++; // only once and step to longer push event
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            if (mode == LcdCanvas::FileView) {
                idx_fast_dec();
            } else if (mode == LcdCanvas::Play) {
                volume_up();
            }
        } else if (button == HP_BUTTON_MINUS) {
            if (mode == LcdCanvas::FileView) {
                idx_fast_inc();
            } else if (mode == LcdCanvas::Play) {
                volume_down();
            }
        }
    } else if (button_repeat_count == 20) { // long long push
        if (button == HP_BUTTON_CENTER) {
            mode_prv = mode;
            mode = LcdCanvas::PowerOff;
        }
        button_repeat_count++; // only once and step to longer push event
    } else if (button == button_prv[0]) {
        button_repeat_count++;
    }
    // Button status shift
    for (i = NUM_BTN_HISTORY-2; i >= 0; i--) {
        button_prv[i+1] = button_prv[i];
    }
    button_prv[0] = button;
    __enable_irq();
    tick_100ms_count++;
}

// Get .mp3/.wav file which idx == idx_play (if seq_flg == 1, successive mp3 is searched)
//   and link play to suitable codec object
AudioCodec *get_audio_file(uint16_t *idx, int seq_flg, MutexFsBaseFile *f)
{
    AudioCodec *next_codec = NULL;
    int flg = 0;
    int ofs = 0;
    char str[256];
    //Serial.print("get_audio_file: ");
    //Serial.println(millis());
    while (*idx + ofs < file_menu_get_num()) {
        file_menu_get_obj(*idx + ofs, f);
        f->getName(str, sizeof(str));
        //file_menu_get_fname(*idx + ofs, str, sizeof(str));
        char* ext_pos = strrchr(str, '.');
        if (ext_pos) {
            if (strncmp(ext_pos, ".mp3", 4) == 0 || strncmp(ext_pos, ".MP3", 4) == 0) {
                next_codec = &playMp3;
                flg = 1;
                break;
            } else if (strncmp(ext_pos, ".wav", 4) == 0 || strncmp(ext_pos, ".WAV", 4) == 0) {
                next_codec = &playWav;
                flg = 1;
                break;
            } else if (strncmp(ext_pos, ".m4a", 4) == 0 || strncmp(ext_pos, ".M4A", 4) == 0) {
                next_codec = &playAac;
                flg = 1;
                break;
            } else if (strncmp(ext_pos, ".flac", 5) == 0 || strncmp(ext_pos, ".FLAC", 5) == 0) {
                next_codec = &playFlac;
                flg = 1;
                break;
            }
        }
        if (!seq_flg) { break; }
        ofs++;
    }
    //Serial.print("get_audio_file done: ");
    //Serial.println(millis());
    if (flg) {
        file_menu_get_fname_UTF16(*idx + ofs, (char16_t *) str, sizeof(str)/2);
        Serial.println(utf16_to_utf8((const char16_t *) str).c_str());
        *idx += ofs;
    } else {
        *idx = 0;
        next_codec = NULL;
    }
    return next_codec;
}

void loadTag(uint16_t idx_play)
{
    char str[256];
    mime_t mime;
    ptype_t ptype;
    uint64_t img_pos;
    size_t size;
    int img_cnt = 0;

    tag.loadFile(idx_play);

    // copy TAG text
    if (tag.getUTF8Track(str, sizeof(str))) lcd.setTrack(str); else lcd.setTrack("");
    if (tag.getUTF8Title(str, sizeof(str))) {
        lcd.setTitle(str, utf8);
    } else { // display filename if no TAG
        file_menu_get_fname_UTF16(idx_play, (char16_t *) str, sizeof(str)/2);
        lcd.setTitle(utf16_to_utf8((const char16_t *) str).c_str(), utf8);
    }
    if (tag.getUTF8Album(str, sizeof(str))) lcd.setAlbum(str, utf8); else lcd.setAlbum("");
    if (tag.getUTF8Artist(str, sizeof(str))) lcd.setArtist(str, utf8); else lcd.setArtist("");
    if (tag.getUTF8Year(str, sizeof(str))) lcd.setYear(str, utf8); else lcd.setYear("");

    // copy TAG image
    lcd.deleteAlbumArt();
    for (int i = 0; i < tag.getPictureCount(); i++) {
        if (tag.getPicturePos(i, &mime, &ptype, &img_pos, &size)) {
            if (mime == jpeg) { lcd.addAlbumArtJpeg(idx_play, img_pos, size); img_cnt++; }
            else if (mime == png) { lcd.addAlbumArtPng(idx_play, img_pos, size); img_cnt++; }
        }
    }
    // if no AlbumArt in TAG, use JPEG or PNG in current folder
    if (img_cnt == 0) {
        uint16_t idx = 0;
        while (idx < file_menu_get_num()) {
            MutexFsBaseFile f;
            file_menu_get_obj(idx, &f);
            f.getName(str, sizeof(str));
            char* ext_pos = strrchr(str, '.');
            if (ext_pos) {
                if (strncmp(ext_pos, ".jpg", 4) == 0  || strncmp(ext_pos, ".JPG", 4) == 0 ||
                    strncmp(ext_pos, ".jpeg", 5) == 0 || strncmp(ext_pos, ".JPEG", 5) == 0) {
                    lcd.addAlbumArtJpeg(idx, 0, f.fileSize());
                    img_cnt++;
                } else if (strncmp(ext_pos, ".png", 4) == 0  || strncmp(ext_pos, ".PNG", 4) == 0) {
                    lcd.addAlbumArtPng(idx, 0, f.fileSize());
                    img_cnt++;
                }
            }
            idx++;
        }
    }
}

void codec_thread()
{
    bool wait_timeout = false;
    while (1) {
        if (!codec_event.wait(100)) {
            if (codec->isPlaying() && !wait_timeout) {
                Serial.println("wait timeout");
                wait_timeout = true;
            } else {
                continue;
            }
        } else {
            codec_event.clear();
            wait_timeout = false;
        }
        decodeMp3_core();
        decodeWav_core();
        decodeAac_core_x2();
        decodeFlac_core_half();
    }
}

void setup()
{
    Serial.begin(115200);
    {
        char str[64];
        Serial.println("###################################");
        sprintf(str, "Teensy 4.0 MP3 Player ver. %d.%02d", Version/100, Version%100);
        Serial.println(str);
        Serial.println("###################################");
    }

    // Pin Mode Setting
    pinMode(PIN_DCDC_SHDN_B, OUTPUT);
    pinMode(PIN_BACKLIGHT_CONTROL, OUTPUT);
    pinMode(PIN_BATTERY_CHECK, OUTPUT);
    analogWrite(PIN_BACKLIGHT_CONTROL, BACKLIGHT_HIGH); // PWM

    // Keep Power On for SHDN_B of DC/DC
    digitalWrite(PIN_DCDC_SHDN_B, HIGH);

    initEEPROM();
    analogReadAveraging(8);
    myTimer.begin(tick_100ms, 100000);
    threads.addThread(codec_thread, 1, 2048);

    stack = stack_init();
    file_menu_open_root_dir();
    Adafruit_GFX::loadUnifontFile("/", "unifont.bin");

    // Audio connections require memory to work.  For more
    // detailed information, see the MemoryAndCpuUsage example
    AudioMemory(15); // 5 for Single MP3

    // Restore power off situation
    loadFromEEPROM();
}

#if 0	
Serial.print("Max Usage: ");
Serial.print(codec->processorUsageMax());
Serial.print("% Audio, ");
Serial.print(codec->processorUsageMaxDecoder());
Serial.print("% Decoding max, ");

Serial.print(codec->processorUsageMaxSD());
Serial.print("% SD max, ");
  
Serial.print(AudioProcessorUsageMax());
Serial.println("% All");

AudioProcessorUsageMaxReset();
codec->processorUsageMaxReset();
codec->processorUsageMaxResetDecoder();
#endif 

void loop()
{
    int i;
    char str[256];
    unsigned long time = millis();
    if (aud_req == 1) { // Play / Pause
        codec->pause(!codec->isPaused());
        aud_req = 0;
        idle_count = 0;
    } else if (aud_req == 2) { // Stop
        codec->stop();
        mode = LcdCanvas::FileView;
        lcd.switchToFileView();
        aud_req = 0;
        idx_req = 1;
    } else if (idx_req_open == 1 && idle_count > 1) { // idle_count for resume play
        if (file_menu_is_dir(idx_head+idx_column) > 0) { // Target is Directory
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
        } else { // Target is File
            file_menu_full_sort();
            if (fpos == 0) { idx_play = idx_head + idx_column; } // fpos == 0: play indicated track,  fpos != 0: use idx_play in EEPROM
            codec = get_audio_file(&idx_play, 0, &file);
            if (codec) { // Play Audio File
                mode = LcdCanvas::Play;
                loadTag(idx_play);
                codec->play(&file, fpos, samples_played); // with resuming file position and play time
                fpos = 0;
                samples_played = 0;
                idle_count = 0;
                lcd.switchToPlay();
            }
        }
        idx_req_open = 0;
    } else if (idx_req_open == 2) { // Random Play
        if (codec->isPlaying()) {
            codec->stop();
            mode = LcdCanvas::FileView;
            lcd.switchToFileView();
        }
        Serial.println("Random Search");
        stack_count = stack_get_count(stack);
        if (stack_count >= 2) { // Random Play for same level folder (Assuming Artist/Alubm/Track)
            for (i = 0; i < 2; i++) {
                file_menu_ch_dir(0); // cd ..
                stack_pop(stack, &item);
            }
            while (1) {
                for (i = 0; i < 2; i++) {
                    if (file_menu_get_dir_num() == 0) { break; }
                    while (1) {
                        idx_head = random(1, file_menu_get_num());
                        file_menu_sort_entry(idx_head, idx_head+1);
                        if (file_menu_is_dir(idx_head) > 0) { break; }
                    }
                    file_menu_ch_dir(idx_head);
                    item.head = idx_head;
                    item.column = 0;
                    stack_push(stack, &item);
                }
                // Check if Next Target Dir has Audio track files
                if (stack_count == stack_get_count(stack) &&
                    (file_menu_get_ext_num("mp3", 3) > 0 || file_menu_get_ext_num("m4a", 3) > 0 || 
                     file_menu_get_ext_num("wav", 3) > 0 || file_menu_get_ext_num("flac", 4) > 0)) {
                    break;
                }
                // Otherwise, chdir to stack_count-2 and retry again
                Serial.println("Retry Random Search");
                while (stack_count - 2 != stack_get_count(stack)) {
                    file_menu_ch_dir(0); // cd ..
                    stack_pop(stack, &item);
                }
            }
            idx_head = 0;
            idx_column = 1;
            idx_req_open = 1;
        } else {
            idx_req_open = 0;
        }
        idx_req = 0;
        idle_count = 0;
    } else if (idx_req) {
        for (i = 0; i < NUM_IDX_ITEMS; i++) {
            if (idx_head+i >= file_menu_get_num()) {
                lcd.setFileItem(i, ""); // delete
                continue;
            }
            if (idx_head+i == 0) {
                lcd.setFileItem(i, "..", file_menu_is_dir(idx_head+i), (i == idx_column));
            } else {
                file_menu_get_fname_UTF16(idx_head+i, (char16_t *) str, sizeof(str)/2);
                lcd.setFileItem(i, utf16_to_utf8((const char16_t *) str).c_str(), file_menu_is_dir(idx_head+i), (i == idx_column), utf8);
            }
        }
        idx_req = 0;
        idle_count = 0;
    } else {
        idle_count++;
        if (mode == LcdCanvas::Play) {
            if (!codec->isPlaying() || (codec->positionMillis() + 500 > codec->lengthMillis())) {
                idx_play++;
                AudioCodec *next_codec = get_audio_file(&idx_play, 1, &file);
                if (next_codec) {
                    loadTag(idx_play);
                    //codec->standby_play(&file);
                    while (codec->isPlaying()) { /*delay(1);*/ }
                    codec = next_codec;
                    codec->play(&file);
                    lcd.switchToPlay();
                } else {
                    while (codec->isPlaying()) { delay(1); } // minimize gap between tracks
                    codec->stop();
                    mode = LcdCanvas::FileView;
                    lcd.switchToFileView();
                    idx_req = 1;
                    idle_count = 0;
                    is_waiting_next_random = true;
                }
            }
            lcd.setVolume(i2s1.get_volume());
            lcd.setBitRate(codec->bitRate());
            lcd.setPlayTime(codec->positionMillis()/1000, codec->lengthMillis()/1000, codec->isPaused());
            if (codec->isPaused() && idle_count > WaitCyclesForPowerOffWhenPaused) {
                mode = LcdCanvas::PowerOff;
            }
        } else if (mode == LcdCanvas::FileView) {
            if (idle_count > 100) {
                file_menu_idle(); // for background sort
            }
            if (is_waiting_next_random && idle_count > WaitCyclesForRandomPlay && stack_get_count(stack) >= 2) {
                idx_req_open = 2; // Random Play
            }
        } else if (mode == LcdCanvas::PowerOff) {
            power_off();
        }
    }
    if (battery_x1000 < 2900) { // Battery Lower Than 2.9V
        power_off("Low Battery");
    }
    lcd.setBatteryVoltage(battery_x1000);
    lcd.draw();
    // Back Light Boost within BackLightBoostTime from last stimulus
    if (idle_count < BackLightBoostCycles) {
        analogWrite(PIN_BACKLIGHT_CONTROL, BACKLIGHT_HIGH); // PWM
    } else {
        analogWrite(PIN_BACKLIGHT_CONTROL, BACKLIGHT_LOW); // PWM
    }
    time = millis() - time;
    if (time < LoopCycleMs) {
        delay(LoopCycleMs - time);
    } else {
        delay(1);
    }
}

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
#include <TeensyThreads.h>

#include <play_sd_mp3.h>
#include <output_i2s.h>
#include "stack.h"
#include "id3read.h"
#include "utf_conv.h"

const int Version = 100;

IntervalTimer myTimer;
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

uint16_t eprw_count; // EEPROM Write Count (to check for write endurance of 100,000 cycles)

// LCD (ST7735, 1.8", 128x160pix)
#define TFT_CS        10
#define TFT_RST        9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC         8

//Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
LcdCanvas lcd = LcdCanvas(TFT_CS, TFT_DC, TFT_RST);

AudioPlaySdMp3    playMp3;
AudioOutputI2S    i2s1;
AudioConnection     patchCord0(playMp3, 0, i2s1, 0);
AudioConnection     patchCord1(playMp3, 1, i2s1, 1);

stack_t *stack; // for change directory history
stack_data_t item;
int stack_count;

// FileView Menu
volatile LcdCanvas::mode_enm mode = LcdCanvas::FileView;
volatile LcdCanvas::mode_enm mode_prv = LcdCanvas::FileView;

#define NUM_IDX_ITEMS         10
volatile int idx_req = 1;
volatile int idx_req_open = 0;
volatile int aud_req = 0;

volatile uint16_t idx_head = 0;
volatile uint16_t idx_column = 0;
uint16_t idx_idle_count = 0;
uint16_t idx_play = 0;
volatile bool is_waiting_next_random = false;

ID3Read id3;

void initEEPROM(void)
{
    char str[64];
    eprw_count = ((uint16_t) EEPROM.read(CFG_EPRW_COUNT_H) << 8) | ((uint16_t) EEPROM.read(CFG_EPRW_COUNT_L));
    if (eprw_count == 0xffff) { // Default Value Clear if area is not initialized
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
    randomSeed(((uint16_t) EEPROM.read(CFG_SEED1) << 8) | ((uint16_t) EEPROM.read(CFG_SEED0)));
    i2s1.set_volume(EEPROM.read(CFG_VOLUME));
    for (int i = EEPROM.read(CFG_STACK_COUNT) - 1; i >= 0; i--) {
        item.head = ((uint16_t) EEPROM.read(CFG_STACK_HEAD0_H + i*4) << 8) | ((uint16_t) EEPROM.read(CFG_STACK_HEAD0_L + i*4));
        item.column = ((uint16_t) EEPROM.read(CFG_STACK_COLUMN0_H + i*4) << 8) | ((uint16_t) EEPROM.read(CFG_STACK_COLUMN0_L + i*4));
        file_menu_sort_entry(item.head+item.column, item.head+item.column + 1);
        if (file_menu_is_dir(item.head+item.column) <= 0 || item.head+item.column == 0) { // Not Directory or Parent Directory
            break;
        }
        stack_push(stack, &item);
        file_menu_ch_dir(item.head+item.column);
    }
    idx_head = ((uint16_t) EEPROM.read(CFG_IDX_HEAD_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_HEAD_L));
    idx_column = ((uint16_t) EEPROM.read(CFG_IDX_COLUMN_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_COLUMN_L));
    mode = static_cast<LcdCanvas::mode_enm>(EEPROM.read(CFG_MODE));
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

void power_off(void)
{
    lcd.switchToPowerOff();
    lcd.draw();
    uint8_t volume = i2s1.get_volume();
    fpos = 0;
    samples_played = 0;
    if (playMp3.isPlaying()) {
        fpos = playMp3.fposition();
        samples_played = playMp3.getSamplesPlayed();
        while (i2s1.get_volume() > 0) {
            i2s1.volume_down();
            delay(5);
            yield(); // Arduino msg loop
        }
        playMp3.stop();
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
    // Self Power Off
    /* do pin control here */
    // Endless Loop
    while (1) {
        delay(100);
        yield(); // Arduino msg loop
    }
}

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
}

void volume_down(void)
{
    i2s1.volume_down();
}

void tick_100ms(void)
{
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
    } else if (button_repeat_count == 30) { // long long push
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
}

// Get .mp3 file which idx == idx_play (if seq_flg == 1, successive mp3 is searched)
int get_mp3_file(uint16_t idx, int seq_flg, MutexFsBaseFile *f)
{
    int flg = 0;
    int ofs = 0;
    char str[256];
    //Serial.print("get_mp3_file: ");
    //Serial.println(millis());
    while (idx + ofs < file_menu_get_num()) {
        file_menu_get_obj(idx + ofs, f);
        f->getName(str, sizeof(str));
        //file_menu_get_fname(idx + ofs, str, sizeof(str));
        char* ext_pos = strrchr(str, '.');
        if (ext_pos) {
            if (strncmp(ext_pos, ".mp3", 4) == 0) {
                Serial.println(str);
                flg = 1;
                break;
            }
        }
        if (!seq_flg) { break; }
        ofs++;
    }
    //Serial.print("get_mp3_file done: ");
    //Serial.println(millis());
    if (flg) {
        return idx + ofs;
    } else {
        return 0;
    }
}

void loadID3(uint16_t idx_play)
{
    char str[256];
    mime_t mime;
    ptype_t ptype;
    uint64_t img_pos;
    size_t size;

    id3.loadFile(idx_play);

    // copy ID3 text
    if (id3.getUTF8Track(str, sizeof(str))) lcd.setTrack(str, utf8); else lcd.setTrack("");
    if (id3.getUTF8Title(str, sizeof(str))) {
        lcd.setTitle(str, utf8);
    } else { // display filename if no ID3
        file_menu_get_fname_UTF16(idx_play, (char16_t *) str, sizeof(str)/2);
        lcd.setTitle(utf16_to_utf8((const char16_t *) str).c_str(), utf8);
    }
    if (id3.getUTF8Album(str, sizeof(str))) lcd.setAlbum(str, utf8); else lcd.setAlbum("");
    if (id3.getUTF8Artist(str, sizeof(str))) lcd.setArtist(str, utf8); else lcd.setArtist("");
    if (id3.getUTF8Year(str, sizeof(str))) lcd.setYear(str, utf8); else lcd.setYear("");

    // copy ID3 image
    lcd.deleteAlbumArt();
    for (int i = 0; i < id3.getPictureCount(); i++) {
        if (id3.getPicturePos(i, &mime, &ptype, &img_pos, &size)) {
            if (mime == jpeg) { lcd.addAlbumArtJpeg(idx_play, img_pos, size); }
            else if (mime == png) { lcd.addAlbumArtPng(idx_play, img_pos, size); }
        }
    }
}

void codec_thread()
{
    while (1) {
        codec_event.wait();
        codec_event.clear();
        decodeMp3_core();
    }
}

void setup() {
    Serial.begin(115200);
    {
        char str[64];
        Serial.println("###################################");
        sprintf(str, "Teensy 4.0 MP3 Player ver. %d.%02d", Version/100, Version%100);
        Serial.println(str);
        Serial.println("###################################");
    }

    initEEPROM();
    myTimer.begin(tick_100ms, 100000);
    threads.addThread(codec_thread, 0, 2048);

    stack = stack_init();
    file_menu_open_root_dir();
    Adafruit_GFX::loadUnifontFile("/", "unifont.bin");

    // Audio connections require memory to work.  For more
    // detailed information, see the MemoryAndCpuUsage example
    AudioMemory(5); // 5 for Single MP3

    // Restore power off situation
    loadFromEEPROM();
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
    unsigned long time = millis();
    if (aud_req == 1) { // Play / Pause
        playMp3.pause(!playMp3.isPaused());
        aud_req = 0;
    } else if (aud_req == 2) { // Stop
        playMp3.stop();
        mode = LcdCanvas::FileView;
        lcd.switchToFileView();
        aud_req = 0;
        idx_req = 1;
    } else if (idx_req_open == 1 && idx_idle_count > 1) { // idx_idle_count for resume play
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
            idx_play = get_mp3_file(idx_play, 0, &file);
            if (idx_play) { // Play Audio File
                mode = LcdCanvas::Play;
                loadID3(idx_play);
                playMp3.play(&file, fpos, samples_played); // with resuming file position and play time
                fpos = 0;
                samples_played = 0;
                idx_idle_count = 0;
                lcd.switchToPlay();
            }
        }
        idx_req_open = 0;
    } else if (idx_req_open == 2) { // Random Play
        if (playMp3.isPlaying()) {
            playMp3.stop();
            mode = LcdCanvas::FileView;
            lcd.switchToFileView();
        }
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
                    (file_menu_get_ext_num("mp3", 3) > 0 || file_menu_get_ext_num("m4a", 3) > 0 || file_menu_get_ext_num("wav", 3) > 0)) {
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
        idx_idle_count = 0;
    } else if (idx_req) {
        for (i = 0; i < NUM_IDX_ITEMS; i++) {
            if (idx_head+i >= file_menu_get_num()) {
                lcd.setFileItem(i, ""); // delete
                continue;
            }
            //file_menu_get_fname(idx_head+i, str, sizeof(str));
            if (idx_head+i == 0) {
                lcd.setFileItem(i, "..", file_menu_is_dir(idx_head+i), (i == idx_column));
            } else {
                file_menu_get_fname_UTF16(idx_head+i, (char16_t *) str, sizeof(str)/2);
                lcd.setFileItem(i, utf16_to_utf8((const char16_t *) str).c_str(), file_menu_is_dir(idx_head+i), (i == idx_column), utf8);
            }
        }
        idx_req = 0;
        idx_idle_count = 0;
    } else {
        if (mode == LcdCanvas::Play) {
            if (!playMp3.isPlaying() || (playMp3.positionMillis() + 500 > playMp3.lengthMillis())) {
                idx_play = get_mp3_file(idx_play+1, 1, &file);
                if (idx_play) {
                    loadID3(idx_play);
                    playMp3.standby_play(&file);
                } else {
                    while (playMp3.isPlaying()) { delay(1); } // minimize gap between tracks
                    playMp3.stop();
                    mode = LcdCanvas::FileView;
                    lcd.switchToFileView();
                    idx_req = 1;
                    idx_idle_count = 0;
                    is_waiting_next_random = true;
                }
            }
            lcd.setVolume(i2s1.get_volume());
            lcd.setBitRate(playMp3.bitRate());
            lcd.setPlayTime(playMp3.positionMillis()/1000, playMp3.lengthMillis()/1000);
        } else if (mode == LcdCanvas::FileView) {
            idx_idle_count++;
            if (idx_idle_count > 100) {
                file_menu_idle();
            }
            if (is_waiting_next_random && idx_idle_count > 20 * 60 * 1 && stack_get_count(stack) >= 2) {
                idx_req_open = 2; // Random Play
            }
        } else if (mode == LcdCanvas::PowerOff) {
            power_off();
        }
    }
    lcd.draw();
    time = millis() - time;
    if (time < 50) {
        delay(50 - time);
    } else {
        delay(1);
    }
}

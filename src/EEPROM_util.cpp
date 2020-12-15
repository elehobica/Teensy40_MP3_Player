/*------------------------------------------------------/
/ EEPROM_util
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "EEPROM_util.h"
#include <EEPROM.h>
#include <file_menu_SdFat.h>
#include "ui_control.h"
#include "audio_playback.h"

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
#define CFG_DISP_ROTATION   (EEPROM_BASE + 45)

static uint16_t eprw_count; // EEPROM Write Count (to check for write endurance of 100,000 cycles)

void initEEPROM()
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
            if (i == CFG_DISP_ROTATION) { continue; }
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

ui_mode_enm_t loadFromEEPROM(LcdCanvas *lcd, stack_t *dir_stack)
{
    uint8_t volume;
    stack_data_t item;
    ui_mode_enm_t ui_mode_enm;
    uint16_t idx_head = 0;
    uint16_t idx_column = 0;
    uint16_t idx_play = 0;
    size_t fpos = 0;
    uint32_t samples_played = 0;
    bool err_flg = false;

    randomSeed(((uint16_t) EEPROM.read(CFG_SEED1) << 8) | ((uint16_t) EEPROM.read(CFG_SEED0)));
    volume = EEPROM.read(CFG_VOLUME);
    if (EEPROM.read(CFG_DISP_ROTATION) < 4) { lcd->setRotation(EEPROM.read(CFG_DISP_ROTATION)); }
    // Resume last folder & play
    for (int i = EEPROM.read(CFG_STACK_COUNT) - 1; i >= 0; i--) {
        item.head = ((uint16_t) EEPROM.read(CFG_STACK_HEAD0_H + i*4) << 8) | ((uint16_t) EEPROM.read(CFG_STACK_HEAD0_L + i*4));
        item.column = ((uint16_t) EEPROM.read(CFG_STACK_COLUMN0_H + i*4) << 8) | ((uint16_t) EEPROM.read(CFG_STACK_COLUMN0_L + i*4));
        if (item.head+item.column >= file_menu_get_num()) { err_flg = true; break; } // idx overflow
        file_menu_sort_entry(item.head+item.column, item.head+item.column + 1);
        if (file_menu_is_dir(item.head+item.column) <= 0 || item.head+item.column == 0) { err_flg = true; break; } // Not Directory or Parent Directory
        stack_push(dir_stack, &item);
        file_menu_ch_dir(item.head+item.column);
    }
    idx_head = ((uint16_t) EEPROM.read(CFG_IDX_HEAD_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_HEAD_L));
    idx_column = ((uint16_t) EEPROM.read(CFG_IDX_COLUMN_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_COLUMN_L));
    ui_mode_enm = static_cast<ui_mode_enm_t>(EEPROM.read(CFG_MODE));
    if (idx_head+idx_column >= file_menu_get_num()) { err_flg = true; } // idx overflow
    if (err_flg) { // Load Error
        stack_delete(dir_stack);
        dir_stack = stack_init();
        file_menu_open_root_dir();
        idx_head = idx_column = 0;
        ui_mode_enm = FileViewMode;
    }
    idx_play = ((uint16_t) EEPROM.read(CFG_IDX_PLAY_H) << 8) | ((uint16_t) EEPROM.read(CFG_IDX_PLAY_L));
    if (ui_mode_enm == PlayMode) {
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
    // Recover ui & audio status
    uiv_set_file_idx(idx_head, idx_column);
    uiv_set_play_idx(idx_play);
    audio_set_volume(volume);
    audio_set_position(fpos, samples_played);

    return ui_mode_enm;
}

void storeToEEPROM(LcdCanvas *lcd, stack_t *dir_stack, ui_mode_enm_t last_ui_mode)
{
    stack_data_t item;
    uint16_t idx_head;
    uint16_t idx_column;
    uint16_t idx_play;
    size_t fpos;
    uint32_t samples_played;

    // Get ui & audio status
    uiv_get_file_idx(&idx_head, &idx_column);
    uiv_get_play_idx(&idx_play);
    audio_get_position(&fpos, &samples_played);

    // Save Config Data to EEPROM
    eprw_count++;
    EEPROM.write(CFG_EPRW_COUNT_L, (uint8_t) (eprw_count & 0xff));
    EEPROM.write(CFG_EPRW_COUNT_H, (uint8_t) ((eprw_count >> 8) & 0xff));
    EEPROM.write(CFG_SEED0, (uint8_t) (millis() & 0xff));
    EEPROM.write(CFG_SEED1, (uint8_t) ((millis() >> 8) & 0xff));
    EEPROM.write(CFG_VOLUME, audio_get_last_volume());
    EEPROM.write(CFG_STACK_COUNT, stack_get_count(dir_stack));
    for (int i = 0; i < EEPROM.read(CFG_STACK_COUNT); i++) {
        stack_pop(dir_stack, &item);
        EEPROM.write(CFG_STACK_HEAD0_L + i*4, (uint8_t) (item.head & 0xff));
        EEPROM.write(CFG_STACK_HEAD0_H + i*4, (uint8_t) ((item.head >> 8) & 0xff));
        EEPROM.write(CFG_STACK_COLUMN0_L + i*4, (uint8_t) (item.column & 0xff));
        EEPROM.write(CFG_STACK_COLUMN0_H + i*4, (uint8_t) ((item.column >> 8) & 0xff));
    }
    EEPROM.write(CFG_IDX_HEAD_L, (uint8_t) (idx_head & 0xff));
    EEPROM.write(CFG_IDX_HEAD_H, (uint8_t) ((idx_head >> 8) & 0xff));
    EEPROM.write(CFG_IDX_COLUMN_L, (uint8_t) (idx_column & 0xff));
    EEPROM.write(CFG_IDX_COLUMN_H, (uint8_t) ((idx_column >> 8) & 0xff));
    EEPROM.write(CFG_MODE, static_cast<uint8_t>(last_ui_mode));
    EEPROM.write(CFG_IDX_PLAY_L, (uint8_t) (idx_play & 0xff));
    EEPROM.write(CFG_IDX_PLAY_H, (uint8_t) ((idx_play >> 8) & 0xff));
    audio_get_position(&fpos, &samples_played);
    uint64_t play_pos = (uint64_t) fpos;
    for (int i = 0; i < 8; i++) {
        EEPROM.write(CFG_PLAY_POS0 + i, (uint8_t) ((play_pos >> i*8) & 0xff));
    }
    for (int i = 0; i < 4; i++) {
        EEPROM.write(CFG_SAMPLES_PLAYED0 + i, (uint8_t) ((samples_played >> i*8) & 0xff));
    }
}

void writeEEPROM(int idx, uint8_t val)
{
    EEPROM.write(idx, val);
}

uint8_t readEEPROM(int idx)
{
    return EEPROM.read(idx);
}
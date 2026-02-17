/*-------------------------------------------------------------------------/
/ Teensy4.0 MP3 Player v0.8.9
/--------------------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/
/ This software is free software: you can redistribute it and/or modify
/ it under the terms of the GNU General Public License as published by
/ the Free Software Foundation, either version 3 of the License, or
/ (at your option) any later version.
/
/ This software is distributed in the hope that it will be useful,
/ but WITHOUT ANY WARRANTY; without even the implied warranty of
/ MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/ GNU General Public License for more details.
/
/ You should have received a copy of the GNU General Public License
/ along with this software.  If not, see <http://www.gnu.org/licenses/>.
/
/ As for the libraries which are used in this software, they can have
/ different license policies, look at the subdirectories of lib directory.
/-------------------------------------------------------------------------*/

#include <Wire.h>
#include <EEPROM.h>
#include <file_menu_SdFat.h>

#include "LcdCanvas.h"
#include "ui_control.h"
#include "stack.h"
#include "EEPROM_util.h"
#include "UserConfig.h"
#include "audio_playback.h"

// Pin Definitions
#define PIN_DAC_MUTE_B          (6)
#define PIN_DCDC_SHDN_B         (16)
#define PIN_BATTERY_CHECK       (18)
#define PIN_BACKLIGHT_CONTROL   (15)

//#define DISABLE_BATTERY_CHECK

// LCD Pin & Parameter Definitions (Edit "#define USE_xxx" in lib/LcdCanvas.h to select LCD type)
#if defined(USE_ST7735_128x160)
// LCD (ST7735, 1.8", 128x160pix)
#define TFT_CS              10
#define TFT_RST             9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC              8
#elif defined(USE_ST7789_240x240_WOCS)
// LCD (ST7789, 1.3", 240x240pix without CS)
#define TFT_CS              -1
#define TFT_RST             9 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC              8
#elif defined(USE_ILI9341_240x320)
// LCD (ILI9341, 2.2", 240x320pix)
#define TFT_CS              10
#define TFT_RST             -1 // Connected to VCC
#define TFT_DC              8
#endif

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-unknown"
#endif
const char *Version = FW_VERSION;

const int LoopCycleMs = UIMode::UpdateCycleMs; // loop cycle (50 ms)
const int OneSec = 1000 / LoopCycleMs; // 1 Sec

IntervalTimer timer;
volatile uint32_t tick_50ms_count = 0;

volatile uint16_t bat_mv = 4200; // battery voltage (mV)

LcdCanvas lcd = LcdCanvas(TFT_CS, TFT_DC, TFT_RST);

// Directory History
stack_t *dir_stack;

void monitor_battery_voltage(uint8_t battery_check_enable_pin, uint8_t battery_voltage_pin)
{
    digitalWrite(battery_check_enable_pin, HIGH);
    delayMicroseconds(100); // waiting for voltage stable
    uint32_t adc0_rdata = analogRead(battery_voltage_pin);
    bat_mv = adc0_rdata * 3300 * (33+10) / 1023 / 33; // voltage divider: 1 Kohm + 3.3 Kohm, ADC ref: 3300mV
    #ifdef BATTERY_VOLTAGE_MSG
    Serial.print("Battery Voltage = ");
    Serial.println(bat_mv);
    #endif // BATTERY_VOLTAGE_MSG
    digitalWrite(battery_check_enable_pin, LOW);
}

void tick_50ms(void)
{
    if (millis() < 1 * 1000) { return; } // not to detect any actions within 1 sec after boot
    __disable_irq(); // need to be below 'if (millis() < 2 * 1000) { return; }' line otherwise system irq will be masked?
    if ((tick_50ms_count % (20*5)) == 0) { // Check Battery Voltage at 5 sec each
        monitor_battery_voltage(PIN_BATTERY_CHECK, PIN_A0);
    }
    update_button_action(PIN_A8);
    tick_50ms_count++;
    __enable_irq();
}

void apply_audio_output_mode(int mode)
{
    switch (mode) {
        case UserConfig::AudioOutI2S:
            digitalWrite(PIN_DAC_MUTE_B, HIGH);
            audio_spdif_mute(true);
            break;
        case UserConfig::AudioOutSPDIF:
            digitalWrite(PIN_DAC_MUTE_B, LOW);
            audio_spdif_mute(false);
            break;
        case UserConfig::AudioOutI2S_SPDIF:
            digitalWrite(PIN_DAC_MUTE_B, HIGH);
            audio_spdif_mute(false);
            break;
        default:
            digitalWrite(PIN_DAC_MUTE_B, HIGH);
            audio_spdif_mute(true);
            break;
    }
}

// terminate() is called from UIPowerOffMode::entry()
void terminate(ui_mode_enm_t resume_ui_mode)
{
    // Audio Termination & Mute
    audio_terminate();
    digitalWrite(PIN_DAC_MUTE_B, LOW);
    audio_spdif_mute(true);
    storeToEEPROM(dir_stack, resume_ui_mode);
    delay(500);
    // Self Power Off
    digitalWrite(PIN_DCDC_SHDN_B, LOW);
    while (1) { // Endless Loop
        yield(); // Arduino msg loop
        delay(100);
    }
}

bool is_battery_low_voltage()
{
    bool flg;
    #if defined(DISABLE_BATTERY_CHECK)
    flg = false;
    #else // defined(DISABLE_BATTERY_CHECK)
    flg = (bat_mv < 2900); // Battery Lower Than 2.9V
    #endif // defined(DISABLE_BATTERY_CHECK)
    uiv_set_battery_voltage(bat_mv, flg);
    return flg;
}

void setup()
{
    Serial.begin(115200);
    {
        char str[64];
        Serial.println("###################################");
        sprintf(str, "Teensy 4.0 MP3 Player ver. %s", Version);
        Serial.println(str);
        Serial.println("###################################");
#if defined(USE_ST7735_128x160)
        Serial.println("LCD: ST7735 128x160");
#elif defined(USE_ST7789_240x240_WOCS)
        Serial.println("LCD: ST7789 240x240");
#elif defined(USE_ILI9341_240x320)
        Serial.println("LCD: ILI9341 240x320");
#endif
    }

    // Pin Mode Setting
    pinMode(PIN_DAC_MUTE_B, OUTPUT);
    pinMode(PIN_DCDC_SHDN_B, OUTPUT);
    pinMode(PIN_BATTERY_CHECK, OUTPUT);
    pinMode(PIN_BACKLIGHT_CONTROL, OUTPUT);
    analogWrite(PIN_BACKLIGHT_CONTROL, USERCFG_DISP_BLH); // PWM

    // Audio Mute
    digitalWrite(PIN_DAC_MUTE_B, LOW);
    // Keep Power On for SHDN_B of DC/DC
    digitalWrite(PIN_DCDC_SHDN_B, HIGH);

    // EEPROM initialize
    initEEPROM();

    // SDCard File system initialize
    dir_stack = stack_init();
    file_menu_open_root_dir();
    // for unicode Font (on SDCard)
    Adafruit_GFX::loadUnifontFile("/", "unifont.bin");

    audio_init();
    // Apply audio output mode from UserConfig
    apply_audio_output_mode(USERCFG_AUD_OUTPUT);
    {
        const char *output_names[] = {"I2S", "S/PDIF", "I2S+S/PDIF"};
        int mode = USERCFG_AUD_OUTPUT;
        if (mode >= 0 && mode <= 2) {
            Serial.print("Audio Output: ");
            Serial.println(output_names[mode]);
        }
    }

    // Restore previous power off situation
    ui_mode_enm_t init_dest_ui_mode = loadFromEEPROM(dir_stack);

    // ADC Average Setting
    analogReadAveraging(1);

    // Timer initialize
    timer.begin(tick_50ms, 50000);

    // UI initialize
    ui_init(init_dest_ui_mode, dir_stack, &lcd);
    ui_reg_terminate_func(terminate);
}

void loop()
{
    unsigned long time = millis();

    // UI process
    if (is_battery_low_voltage()) {
        ui_force_update(PowerOffMode);
    } else {
        ui_update();
    }

    // Back Light Boost within BackLightBoostTime from last stimulus
    if (USERCFG_DISP_TM_BLL < 0 || ui_get_idle_count() < USERCFG_DISP_TM_BLL*OneSec) {
        analogWrite(PIN_BACKLIGHT_CONTROL, USERCFG_DISP_BLH); // PWM
    } else {
        analogWrite(PIN_BACKLIGHT_CONTROL, USERCFG_DISP_BLL); // PWM
    }

    time = millis() - time;
    if (time < LoopCycleMs) {
        delay(LoopCycleMs - time);
    } else {
        delay(1);
    }
}
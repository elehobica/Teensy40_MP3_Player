/*------------------------------------------------------/
/ ui_control
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#include "ui_control.h"
#include <Wire.h>
#include <TeensyThreads.h>

#define NUM_BTN_HISTORY   30
#define HP_BUTTON_OPEN    0
#define HP_BUTTON_CENTER  1
#define HP_BUTTON_D       2
#define HP_BUTTON_PLUS    3
#define HP_BUTTON_MINUS   4

#define RELEASE_IGNORE_COUNT    8

static uint8_t button_prv[NUM_BTN_HISTORY] = {}; // initialized as HP_BUTTON_OPEN
static uint32_t button_repeat_count = 0;
static bool fwd_lock = false;
static bool rwd_lock = false;

Threads::Event button_event;
volatile button_action_t button_action;

UIVars vars;
UIMode *ui_mode = NULL;
UIMode *ui_mode_ary[4] = {};

void (*_terminate)(ui_mode_enm_t last_ui_mode) = NULL;

static uint8_t adc0_get_hp_button(uint8_t android_MIC_pin)
{
    uint8_t ret;
    const int max = (1<<10) - 1;
    uint16_t adc0_rdata = analogRead(android_MIC_pin);
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
    for (i = 0; i < RELEASE_IGNORE_COUNT; i++) { // Ignore Wait 50ms * RELEASE_IGNORE_COUNT from last action
        if (button_prv[i] != HP_BUTTON_OPEN) {
            return 0;
        }
    }
    for (i = RELEASE_IGNORE_COUNT; i < NUM_BTN_HISTORY; i++) {
        if (detected_fall == 0 && button_prv[i-1] == HP_BUTTON_OPEN && button_prv[i] == HP_BUTTON_CENTER) {
            detected_fall = 1;
        } else if (detected_fall == 1 && button_prv[i-1] == HP_BUTTON_CENTER && button_prv[i] == HP_BUTTON_OPEN) {
            count++;
            detected_fall = 0;
        }
    }
    if (count > 0) {
        /*
        { // DEBUG
            char str[64];
            sprintf(str, "center_clicks = %d", center_clicks);
            Serial.println(str);
        }
        */
        /*{ // DEBUG
            char str[NUM_BTN_HISTORY+1] = {};
            for (i = 0; i < NUM_BTN_HISTORY; i++) {
                str[i] = 'A' + button_prv[i] - 1;
            }
            Serial.println(str);
        }*/
        for (i = 0; i < NUM_BTN_HISTORY; i++) button_prv[i] = HP_BUTTON_OPEN;
    }
    return count;
}

void update_button_action(uint8_t android_MIC_pin)
{
    int i;
    int center_clicks;
    uint8_t button = adc0_get_hp_button(android_MIC_pin);
    if (fwd_lock) {
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            button_action = ButtonPlusFwd;
            button_event.trigger();
        } else {
            fwd_lock = false;
        }
    } else if (rwd_lock) {
        if (button == HP_BUTTON_MINUS) {
            button_action = ButtonMinusRwd;
            button_event.trigger();
        } else {
            rwd_lock = false;
        }
    } else if (button_prv[0] == HP_BUTTON_CENTER && button != HP_BUTTON_CENTER) { // center release with plus or minus pushed
        button_repeat_count = 0;
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            fwd_lock = true;
        } else if (button == HP_BUTTON_MINUS) {
            rwd_lock = true;
        }
    } else if (button == HP_BUTTON_OPEN && button_prv[RELEASE_IGNORE_COUNT] == HP_BUTTON_CENTER) { // center release
        button_repeat_count = 0;
        center_clicks = count_center_clicks(); // must be called once per tick because button_prv[] status has changed
        switch (center_clicks) {
            case 1:
                button_action = ButtonCenterSingle;
                button_event.trigger();
                break;
            case 2:
                button_action = ButtonCenterDouble;
                button_event.trigger();
                break;
            case 3:
                button_action = ButtonCenterTriple;
                button_event.trigger();
                break;
            default:
                break;
        }
    } else if (button_prv[0] == HP_BUTTON_OPEN) { // push
        if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            button_action = ButtonPlusSingle;
            button_event.trigger();
        } else if (button == HP_BUTTON_MINUS) {
            button_action = ButtonMinusSingle;
            button_event.trigger();
        }
    } else if (button_repeat_count == 10) { // long push
        if (button == HP_BUTTON_CENTER) {
            button_repeat_count++; // only once and step to longer push event
        } else if (button == HP_BUTTON_D || button == HP_BUTTON_PLUS) {
            button_action = ButtonPlusLong;
            button_event.trigger();
        } else if (button == HP_BUTTON_MINUS) {
            button_action = ButtonMinusLong;
            button_event.trigger();
        }
    } else if (button_repeat_count == 20) { // long long push
        if (button == HP_BUTTON_CENTER) {
            button_action = ButtonCenterLongLong;
            button_event.trigger();
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
}

UIMode *getUIMode(ui_mode_enm_t ui_mode_enm)
{
    return ui_mode_ary[ui_mode_enm];
}

void ui_init(ui_mode_enm_t init_dest_ui_mode, LcdCanvas *lcd, stack_t *dir_stack, const uint16_t num_list_lines)
{
    vars.init_dest_ui_mode = init_dest_ui_mode;
    UIMode::linkLcdCanvas(lcd);
    UIMode::linkButtonAction(&button_event, &button_action);
    vars.num_list_lines = num_list_lines;

    ui_mode_ary[InitialMode]  = (UIMode *) new UIInitialMode(&vars);
    ui_mode_ary[FileViewMode] = (UIMode *) new UIFileViewMode(&vars, dir_stack);
    ui_mode_ary[PlayMode]     = (UIMode *) new UIPlayMode(&vars);
    ui_mode_ary[PowerOffMode] = (UIMode *) new UIPowerOffMode(&vars);
    ui_mode = getUIMode(InitialMode);
    ui_mode->entry(ui_mode);
}

void ui_update()
{
    //Serial.println(ui_mode->getName());
    UIMode *ui_mode_next = ui_mode->update();
    if (ui_mode_next != ui_mode) {
        ui_mode_next->entry(ui_mode);
        ui_mode = ui_mode_next;
    } else {
        ui_mode->draw();
    }
}

void ui_force_update(ui_mode_enm_t ui_mode_enm)
{
    //Serial.println(ui_mode->getName());
    UIMode *ui_mode_next = getUIMode(ui_mode_enm);
    ui_mode->update();
    if (ui_mode_next != ui_mode) {
        ui_mode_next->entry(ui_mode);
        ui_mode = ui_mode_next;
    } else {
        ui_mode->draw();
    }
}

void ui_reg_terminate_func(void (*terminate)(ui_mode_enm_t last_ui_mode))
{
    _terminate = terminate;
}

void ui_terminate(ui_mode_enm_t last_ui_mode)
{
    if (_terminate == NULL) { return; }
    (*_terminate)(last_ui_mode);
}

uint16_t ui_get_idle_count()
{
    return ui_mode->getIdleCount();
}

void uiv_set_battery_voltage(uint16_t bat_mv, bool is_low)
{
    if (is_low) { vars.power_off_msg = "Low Battery"; }
    vars.bat_mv = bat_mv;
}

void uiv_set_file_idx(uint16_t idx_head, uint16_t idx_column)
{
    vars.idx_head = idx_head;
    vars.idx_column = idx_column;
}

void uiv_get_file_idx(uint16_t *idx_head, uint16_t *idx_column)
{
    *idx_head = vars.idx_head;
    *idx_column = vars.idx_column;
}

void uiv_set_play_idx(uint16_t idx_play)
{
    vars.idx_play = idx_play;
}

void uiv_get_play_idx(uint16_t *idx_play)
{
    *idx_play = vars.idx_play;
}

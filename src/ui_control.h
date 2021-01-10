/*------------------------------------------------------/
/ ui_control
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#ifndef __UI_CONTROL_H__
#define __UI_CONTROL_H__

#include "UIMode.h"
#include "LcdCanvas.h"
#include "stack.h"

void update_button_action(uint8_t android_MIC_pin);

UIMode *getUIMode(ui_mode_enm_t ui_mode_enm);

void ui_init(ui_mode_enm_t init_dest_ui_mode, LcdCanvas *lcd, stack_t *dir_stack, const uint16_t num_list_lines);
void ui_update();
void ui_force_update(ui_mode_enm_t ui_mode_enm);
void ui_reg_terminate_func(void (*terminate)(ui_mode_enm_t last_ui_mode));
void ui_terminate(ui_mode_enm_t last_ui_mode);
uint16_t ui_get_idle_count();

void uiv_set_battery_voltage(uint16_t bat_mv, bool is_low);
void uiv_set_file_idx(uint16_t idx_head, uint16_t idx_column);
void uiv_get_file_idx(uint16_t *idx_head, uint16_t *idx_column);
void uiv_set_play_idx(uint16_t idx_play);
void uiv_get_play_idx(uint16_t *idx_play);

#endif // __UI_CONTROL_H__
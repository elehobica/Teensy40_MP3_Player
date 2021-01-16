/*------------------------------------------------------/
/ EEPROM_util
/-------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

#ifndef __EEPROM_UTIL_H__
#define __EEPROM_UTIL_H__

#include <stdint.h>
#include "LcdCanvas.h"
#include "stack.h"
#include "UIMode.h"

void initEEPROM();
ui_mode_enm_t loadFromEEPROM(stack_t *dir_stack);
void storeToEEPROM(stack_t *dir_stack, ui_mode_enm_t last_ui_mode);
void loadUserConfigFromEEPROM(int idx, int *val);
void storeUserConfigToEEPROM(int idx, int *val);

#endif // __EEPROM_UTIL_H__
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
ui_mode_enm_t loadFromEEPROM(LcdCanvas *lcd, stack_t *dir_stack);
void storeToEEPROM(LcdCanvas *lcd, stack_t *dir_stack, ui_mode_enm_t last_ui_mode);
void writeEEPROM(int idx, uint8_t val);
uint8_t readEEPROM(int idx);

#endif // __EEPROM_UTIL_H__
/* Copyright 2023 fagci
 * https://github.com/fagci
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include "../bsp/dp32g030/gpio.h"
#include "../driver/backlight.h"
#include "../driver/bk4819-regs.h"
#include "../driver/bk4819.h"
#include "../driver/bk1080-regs.h"
#include "../driver/bk1080.h"
#include "../driver/gpio.h"
#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../external/printf/printf.h"
#include "../frequencies.h"
#include "../ui/helper.h"
#include "../font.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../driver/eeprom.h"
#include "settings.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void APP_RunSpectrum(void);

#endif /* ifndef SPECTRUM_H */

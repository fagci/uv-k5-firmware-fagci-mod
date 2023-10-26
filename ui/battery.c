/* Copyright 2023 Dual Tachyon
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

#include "battery.h"
#include "../driver/st7565.h"
#include "../helper/battery.h"

void drawBat(uint8_t Level, uint8_t start) {
  const uint8_t WORK_START = start + 2;
  const uint8_t WORK_WIDTH = 10;
  const uint8_t WORK_END = WORK_START + WORK_WIDTH;

  gStatusLine[start] |= 0b000001110;
  gStatusLine[start + 1] |= 0b000011111;
  gStatusLine[WORK_END] |= 0b000011111;

  Level <<= 1;

  for (uint8_t i = 1; i <= WORK_WIDTH; ++i) {
    if (Level >= i) {
      gStatusLine[WORK_END - i] |= 0b000011111;
    } else {
      gStatusLine[WORK_END - i] |= 0b000010001;
    }
  }

  if (gChargingWithTypeC) {
    gStatusLine[WORK_START + 1] &= 0b11110111;
    gStatusLine[WORK_START + 2] &= 0b11110111;
    gStatusLine[WORK_START + 3] &= 0b11110111;
    gStatusLine[WORK_START + 4] &= 0b11110111;
    gStatusLine[WORK_START + 5] &= 0b11110111;

    gStatusLine[WORK_START + 4] &= 0b11111011;

    gStatusLine[WORK_START + 3] &= 0b11111101;
    gStatusLine[WORK_START + 4] &= 0b11111101;
    gStatusLine[WORK_START + 5] &= 0b11111101;
    gStatusLine[WORK_START + 6] &= 0b11111101;
    gStatusLine[WORK_START + 7] &= 0b11111101;
    gStatusLine[WORK_START + 8] &= 0b11111101;
  }
}

void UI_DisplayBattery(uint8_t Level) { drawBat(Level, 115); }

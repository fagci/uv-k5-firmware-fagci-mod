/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
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

#include "rssi.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../settings.h"
#include "../ui/helper.h"
#include <string.h>

void UI_DisplayRSSIBar(int16_t rssi) {
  char String[16];

  const uint8_t LINE = 3;
  const uint8_t BAR_LEFT_MARGIN = 24;

  int dBm = Rssi2DBm(rssi);
  uint8_t s = DBm2S(dBm);
  uint8_t *line = gFrameBuffer[LINE];

  memset(line, 0, 128);

  for (int i = BAR_LEFT_MARGIN, sv = 1; i < BAR_LEFT_MARGIN + s * 4;
       i += 4, sv++) {
    line[i] = line[i + 2] = 0b00111110;
    line[i + 1] = sv > 9 ? 0b00100010 : 0b00111110;
  }

  sprintf(String, "%d", dBm);
  UI_PrintStringSmallest(String, 110, 25, false, true);
  if (s < 10) {
    sprintf(String, "S%u", s);
  } else {
    sprintf(String, "S9+%u0", s - 9);
  }
  UI_PrintStringSmallest(String, 3, 25, false, true);
  ST7565_BlitFullScreen();
}

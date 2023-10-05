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

#include "battery.h"
#include "../helper/measurements.h"
#include "driver/backlight.h"
#include "misc.h"
#include "ui/battery.h"
#include "ui/menu.h"
#include "ui/ui.h"

uint16_t gBatteryCalibration[6];
uint16_t gBatteryCurrentVoltage;
uint16_t gBatteryCurrent;
uint16_t gBatteryVoltages[4];
uint16_t gBatteryVoltageAverage;

uint8_t gBatteryDisplayLevel;

bool gChargingWithTypeC;
bool gLowBattery;
bool gLowBatteryBlink;

volatile uint16_t gBatterySave;

uint16_t gBatteryCheckCounter;

void BATTERY_GetReadings(bool bDisplayBatteryLevel) {
  uint8_t PreviousBatteryLevel = gBatteryDisplayLevel;
  uint16_t Voltage = Mid(gBatteryVoltages, ARRAY_SIZE(gBatteryVoltages));

  gBatteryDisplayLevel = 0;
  for (int i = ARRAY_SIZE(gBatteryCalibration) - 1; i >= 0; --i) {
    if (gBatteryCalibration[i] < Voltage) {
      gBatteryDisplayLevel = i + 1;
      break;
    }
  }

  gBatteryVoltageAverage = (Voltage * 760) / gBatteryCalibration[3];

  if ((gScreenToDisplay == DISPLAY_MENU) && gMenuCursor == MENU_VOL) {
    gUpdateDisplay = true;
  }
  if (gBatteryCurrent < 501) {
    if (gChargingWithTypeC) {
      gUpdateStatus = true;
    }
    gChargingWithTypeC = false;
  } else {
    if (!gChargingWithTypeC) {
      gUpdateStatus = true;
      BACKLIGHT_TurnOn();
    }
    gChargingWithTypeC = true;
  }

  if (PreviousBatteryLevel != gBatteryDisplayLevel) {
    if (gBatteryDisplayLevel < 2) {
      gLowBattery = true;
    } else {
      gLowBattery = false;
      if (bDisplayBatteryLevel) {
        UI_DisplayBattery(gBatteryDisplayLevel);
      }
    }
    gLowBatteryCountdown = 0;
  }
}

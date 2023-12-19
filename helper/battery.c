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



#include <assert.h>
#include "battery.h"
#include "driver/backlight.h"
#include "driver/st7565.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include "../helper/measurements.h"

uint16_t gBatteryCalibration[6];
uint16_t gBatteryCurrentVoltage;
uint16_t gBatteryCurrent;
uint16_t gBatteryVoltages[4];
uint16_t gBatteryVoltageAverage;
uint16_t          gBatteryVoltageAverage;
uint8_t           gBatteryDisplayLevel;
bool              gChargingWithTypeC;
bool              gLowBatteryBlink;
bool              gLowBattery;
bool              gLowBatteryConfirmed;
uint16_t          gBatteryCheckCounter;
volatile uint16_t gBatterySave;


typedef enum {
	BATTERY_LOW_INACTIVE,
	BATTERY_LOW_ACTIVE,
	BATTERY_LOW_CONFIRMED
} BatteryLow_t;


uint16_t          lowBatteryCountdown;
const uint16_t 	  lowBatteryPeriod = 30;

volatile uint16_t gPowerSave_10ms;


const uint16_t Voltage2PercentageTable[][7][2] = {
	[BATTERY_TYPE_1600_MAH] = {
		{828, 100},
		{814, 97 },
		{760, 25 },
		{729, 6  },
		{630, 0  },
		{0,   0  },
		{0,   0  },
	},

	[BATTERY_TYPE_2200_MAH] = {
		{832, 100},
		{813, 95 },
		{740, 60 },
		{707, 21 },
		{682, 5  },
		{630, 0  },
		{0,   0  },
	},
};

static_assert(ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_1600_MAH]) ==
	ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_2200_MAH]));


unsigned int BATTERY_VoltsToPercent(const unsigned int voltage_10mV)
{
	const uint16_t (*crv)[2] = Voltage2PercentageTable[gEeprom.BATTERY_TYPE];
	const int mulipl = 1000;
	for (unsigned int i = 1; i < ARRAY_SIZE(Voltage2PercentageTable[BATTERY_TYPE_2200_MAH]); i++) {
		if (voltage_10mV > crv[i][0]) {
			const int a = (crv[i - 1][1] - crv[i][1]) * mulipl / (crv[i - 1][0] - crv[i][0]);
			const int b = crv[i][1] - a * crv[i][0] / mulipl;
			const int p = a * voltage_10mV / mulipl + b;
			return MIN(p, 100);
		}
	}

	return 0;
}

void BATTERY_GetReadings(bool bDisplayBatteryLevel) {
  uint8_t PreviousBatteryLevel = gBatteryDisplayLevel;
  uint16_t Voltage = Mid(gBatteryVoltages, ARRAY_SIZE(gBatteryVoltages));

  gBatteryDisplayLevel = 0;
  for (int i = ARRAY_SIZE(gBatteryCalibration) - 1; i >= 0; --i) {
    if (Voltage > gBatteryCalibration[i]) {
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

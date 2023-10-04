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

#include "../bitmaps.h"
#include "../board.h"
#include "../bsp/dp32g030/gpio.h"
#include "../driver/bk4819-regs.h"
#include "../driver/bk4819.h"
#include "../driver/gpio.h"
#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../frequencies.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/helper.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const uint8_t DrawingEndY = 40;

static const uint8_t U8RssiMap[] = {
    141, 135, 129, 123, 117, 111, 105, 99, 93, 83, 73, 63,
};

static const uint16_t scanStepValues[] = {
    1,   10,  50,  100,

    250, 500, 625, 833, 1000, 1250, 2500, 10000,
};

static const uint8_t gStepSettingToIndex[] = {
    [STEP_2_5kHz] = 4,  [STEP_5_0kHz] = 5,  [STEP_6_25kHz] = 6,
    [STEP_10_0kHz] = 8, [STEP_12_5kHz] = 9, [STEP_25_0kHz] = 10,
    [STEP_8_33kHz] = 7,
};

static const uint16_t scanStepBWRegValues[] = {
    //     RX  RXw TX  BW
    // 0b0 000 000 001 01 1000
    // 1
    0b0000000001011000, // 6.25
    // 10
    0b0000000001011000, // 6.25
    // 50
    0b0000000001011000, // 6.25
    // 100
    0b0000000001011000, // 6.25
    // 250
    0b0000000001011000, // 6.25
    // 500
    0b0010010001011000, // 6.25
    // 625
    0b0100100001011000, // 6.25
    // 833
    0b0110110001001000, // 6.25
    // 1000
    0b0110110001001000, // 6.25
    // 1250
    0b0111111100001000, // 6.25
    // 2500
    0b0011011000101000, // 25
    // 10000
    0b0011011000101000, // 25
};

static const uint16_t listenBWRegValues[] = {
    0b0011011000101000, // 25
    0b0111111100001000, // 12.5
    0b0100100001011000, // 6.25
};

typedef enum State {
  SPECTRUM,
  FREQ_INPUT,
  STILL,
} State;

typedef enum StepsCount {
  STEPS_128,
  STEPS_64,
  STEPS_32,
  STEPS_16,
} StepsCount;

typedef enum ModulationType {
  MOD_FM,
  MOD_AM,
  MOD_USB,
} ModulationType;

typedef enum ScanStep {
  S_STEP_0_01kHz,
  S_STEP_0_1kHz,
  S_STEP_0_5kHz,
  S_STEP_1_0kHz,

  S_STEP_2_5kHz,
  S_STEP_5_0kHz,
  S_STEP_6_25kHz,
  S_STEP_8_33kHz,
  S_STEP_10_0kHz,
  S_STEP_12_5kHz,
  S_STEP_25_0kHz,
  S_STEP_100_0kHz,
} ScanStep;

typedef struct SpectrumSettings {
  StepsCount stepsCount;
  ScanStep scanStepIndex;
  uint32_t frequencyChangeStep;
  uint16_t scanDelay;
  uint16_t rssiTriggerLevel;

  bool backlightState;
  BK4819_FilterBandwidth_t bw;
  BK4819_FilterBandwidth_t listenBw;
  ModulationType modulationType;
} SpectrumSettings;

typedef struct KeyboardState {
  KEY_Code_t current;
  KEY_Code_t prev;
  uint8_t counter;
} KeyboardState;

typedef struct ScanInfo {
  uint16_t rssi, rssiMin, rssiMax;
  uint8_t i, iPeak;
  uint32_t f, fPeak;
  uint16_t scanStep;
  uint8_t measurementsCount;
} ScanInfo;

typedef struct RegisterSpec {
  char *name;
  uint8_t num;
  uint8_t offset;
  uint16_t maxValue;
  uint16_t inc;
} RegisterSpec;

typedef struct PeakInfo {
  uint16_t t;
  uint16_t rssi;
  uint8_t i;
  uint32_t f;
} PeakInfo;

typedef struct FreqPreset {
  const char name[16];
  const uint32_t fStart;
  const uint32_t fEnd;
  const StepsCount stepsCountIndex;
  const uint8_t stepSizeIndex;
  const ModulationType modulationType;
  const BK4819_FilterBandwidth_t listenBW;
} FreqPreset;

static const FreqPreset freqPresets[] = {
    {"CB", 2697500, 2785500, STEPS_128, S_STEP_5_0kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"17m", 1806800, 1831800, STEPS_128, S_STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"15m", 2100000, 2145000, STEPS_128, S_STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"12m", 2489000, 2514000, STEPS_128, S_STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"10m", 2800000, 2970000, STEPS_128, S_STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"2m", 14400000, 14600000, STEPS_128, S_STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"AIR", 11800000, 13500000, STEPS_128, S_STEP_100_0kHz, MOD_AM,
     BK4819_FILTER_BW_NARROW},
    {"JD1", 15175000, 15400000, STEPS_128, S_STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"JD2", 15500000, 15600000, STEPS_64, S_STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"LPD", 43307500, 43477500, STEPS_128, S_STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"PMR", 44600625, 44620000, STEPS_32, S_STEP_6_25kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
};

void APP_RunSpectrum(void);

#endif /* ifndef SPECTRUM_H */

// vim: ft=c

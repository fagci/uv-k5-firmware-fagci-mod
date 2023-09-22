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

#include "app/spectrum.h"
#include "bsp/dp32g030/gpio.h"
#include "driver/bk1080.h"
#include "driver/bk4819-regs.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/keyboard.h"
#include "driver/st7565.h"
#include "driver/system.h"
#include "driver/systick.h"
#include "external/printf/printf.h"
#include "font.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include <stdint.h>
#include <string.h>

static uint16_t R30, R37, R3D, R43, R47, R48, R4B, R7E;
const static uint32_t F_MIN = 0;
const static uint32_t F_MAX = 130000000;

const static uint32_t F_BFM_MIN = 7600000;
const static uint32_t F_BFM_MAX = 10800000;

enum State {
  SPECTRUM,
  FREQ_INPUT,
  STILL,
} currentState = SPECTRUM,
  previousState = SPECTRUM;

struct PeakInfo {
  uint16_t t;
  uint8_t rssi;
  uint8_t i;
  uint32_t f;
} peak;

enum StepsCount {
  STEPS_128,
  STEPS_64,
  STEPS_32,
  STEPS_16,
};

typedef enum ModulationType {
  MOD_FM,
  MOD_AM,
  MOD_USB,
} ModulationType;

enum ScanStep {
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
};

uint16_t scanStepValues[] = {
    1,   10,  50,  100,

    250, 500, 625, 833, 1000, 1250, 2500, 10000,
};

enum MenuState {
  MENU_OFF,
  MENU_AFDAC,
  MENU_PGA,
  MENU_MIXER,
  MENU_LNA,
  MENU_LNA_SHORT,
  MENU_IF,
  MENU_RF,
  MENU_RFW,
  MENU_AGC_FIX_MODE,
  MENU_AGC_FIX_INDEX,
  MENU_AGCDC,
} menuState;

char *menuItems[] = {
    "",   "AFDAC", "PGA",  "MIXER", "LNA",  "LNAS",
    "IF", "RF",    "RFWe", "AGCM",  "AGCI", "AGCDC",
};

static uint16_t GetRegMenuValue(enum MenuState st) {
  switch (st) {
  case MENU_AFDAC:
    return BK4819_ReadRegister(0x48) & 0b1111;
  case MENU_PGA:
    return BK4819_ReadRegister(0x13) & 0b111;
  case MENU_MIXER:
    return (BK4819_ReadRegister(0x13) >> 3) & 0b11;
  case MENU_LNA:
    return (BK4819_ReadRegister(0x13) >> 5) & 0b111;
  case MENU_LNA_SHORT:
    return (BK4819_ReadRegister(0x13) >> 8) & 0b11;
  case MENU_IF:
    return BK4819_ReadRegister(0x3D);
  case MENU_RF:
    return (BK4819_ReadRegister(0x43) >> 12) & 0b111;
  case MENU_RFW:
    return (BK4819_ReadRegister(0x43) >> 9) & 0b111;
  case MENU_AGC_FIX_MODE:
    return (BK4819_ReadRegister(0x7E) >> 15) & 0b1;
  case MENU_AGC_FIX_INDEX:
    return (BK4819_ReadRegister(0x7E) >> 12) & 0b111;
  case MENU_AGCDC:
    return BK4819_ReadRegister(0x7E) & 0b111;
  default:
    return 0;
  }
}

static void SetRegMenuValue(enum MenuState st, bool add) {
  uint16_t v = GetRegMenuValue(st);
  uint16_t vmin = 0, vmax;
  uint8_t regnum = 0;
  uint8_t offset = 0;
  uint16_t inc = 1;
  switch (st) {
  case MENU_AFDAC:
    regnum = 0x48;
    vmax = 0b1111;
    break;
  case MENU_PGA:
    regnum = 0x13;
    vmax = 0b111;
    break;
  case MENU_MIXER:
    regnum = 0x13;
    vmax = 0b11;
    offset = 3;
    break;
  case MENU_LNA:
    regnum = 0x13;
    vmax = 0b111;
    offset = 5;
    break;
  case MENU_LNA_SHORT:
    regnum = 0x13;
    vmax = 0b11;
    offset = 8;
    break;
  case MENU_IF:
    regnum = 0x3D;
    vmax = 0xFFFF;
    inc = 0x2aab;
    break;
  case MENU_RF:
    regnum = 0x43;
    vmax = 0b111;
    offset = 12;
    break;
  case MENU_RFW:
    regnum = 0x43;
    vmax = 0b111;
    offset = 9;
    break;
  case MENU_AGC_FIX_MODE:
    regnum = 0x7E;
    vmax = 0b1;
    offset = 15;
    break;
  case MENU_AGC_FIX_INDEX:
    regnum = 0x7E;
    vmax = 0b111;
    offset = 12;
    break;
  case MENU_AGCDC:
    regnum = 0x7E;
    vmax = 0b111;
    break;
  default:
    return;
  }
  uint16_t reg = BK4819_ReadRegister(regnum);
  if (add && v < vmax) {
    v += inc;
  }
  if (!add && v > vmin) {
    v -= inc;
  }
  reg &= ~(vmax << offset);
  BK4819_WriteRegister(regnum, reg | (v << offset));
}

void SetState(enum State state) {
  previousState = currentState;
  currentState = state;
}

char *bwOptions[] = {"25k", "12.5k", "6.25k"};
char *modulationTypeOptions[] = {"FM", "AM", "USB"};

struct SpectrumSettings {
  enum StepsCount stepsCount;
  enum ScanStep scanStepIndex;
  uint32_t frequencyChangeStep;
  uint16_t scanDelay;
  uint8_t rssiTriggerLevel;

  bool backlightState;
  BK4819_FilterBandwidth_t bw;
  BK4819_FilterBandwidth_t listenBw;
  ModulationType modulationType;
} settings = {STEPS_64,
              S_STEP_25_0kHz,
              80000,
              800,
              0,
              true,
              BK4819_FILTER_BW_WIDE,
              BK4819_FILTER_BW_WIDE,
              false};

static const uint8_t DrawingEndY = 42;

uint8_t rssiHistory[128] = {};

struct ScanInfo {
  uint8_t rssi, rssiMin, rssiMax;
  uint8_t i, iPeak;
  uint32_t f, fPeak;
  uint16_t scanStep;
  uint8_t measurementsCount;
} scanInfo;
uint16_t listenT = 0;

KEY_Code_t btn;
uint8_t btnCounter = 0;
uint8_t btnPrev;
uint32_t currentFreq, tempFreq;
uint8_t freqInputIndex = 0;
KEY_Code_t freqInputArr[10];

bool isInitialized;

bool isListening = false;
bool redrawScreen = true;
bool preventKeypress = true;

// GUI functions

static void PutPixel(uint8_t x, uint8_t y, bool fill) {
  if (fill) {
    gFrameBuffer[y >> 3][x] |= 1 << (y & 7);
  } else {
    gFrameBuffer[y >> 3][x] &= ~(1 << (y & 7));
  }
}
static void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
  if (fill) {
    gStatusLine[x] |= 1 << y;
  } else {
    gStatusLine[x] &= ~(1 << y);
  }
}

static void DrawHLine(int sy, int ey, int nx, bool fill) {
  for (int i = sy; i <= ey; i++) {
    if (i < 56 && nx < 128) {
      PutPixel(nx, i, fill);
    }
  }
}

static void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                bool statusbar, bool fill) {
  uint8_t c;
  uint8_t pixels;
  const uint8_t *p = (const uint8_t *)pString;

  while ((c = *p++) && c != '\0') {
    c -= 0x20;
    for (int i = 0; i < 3; ++i) {
      pixels = gFont3x5[c][i];
      for (int j = 0; j < 6; ++j) {
        if (pixels & 1) {
          if (statusbar)
            PutPixelStatus(x + i, y + j, fill);
          else
            PutPixel(x + i, y + j, fill);
        }
        pixels >>= 1;
      }
    }
    x += 4;
  }
}

// Utility functions

KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  if (btn == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
    btn = KEY_PTT;
  }
  return btn;
}

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

static uint8_t my_abs(signed v) { return v > 0 ? v : -v; }

// Radio functions

static bool IsBroadcastFM(uint32_t f) {
  return f >= F_BFM_MIN && f <= F_BFM_MAX;
}

static void ToggleAFBit(bool on) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  reg &= ~(1 << 8);
  if (on)
    reg |= on << 8;
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void BackupRegisters() {
  R30 = BK4819_ReadRegister(0x30);
  R37 = BK4819_ReadRegister(0x37);
  R3D = BK4819_ReadRegister(0x3D);
  R43 = BK4819_ReadRegister(0x43);
  R47 = BK4819_ReadRegister(0x47);
  R48 = BK4819_ReadRegister(0x48);
  R4B = BK4819_ReadRegister(0x4B);
  R7E = BK4819_ReadRegister(0x7E);
}

static void RestoreRegisters() {
  BK4819_WriteRegister(0x30, R30);
  BK4819_WriteRegister(0x37, R37);
  BK4819_WriteRegister(0x3D, R3D);
  BK4819_WriteRegister(0x43, R43);
  BK4819_WriteRegister(0x47, R47);
  BK4819_WriteRegister(0x48, R48);
  BK4819_WriteRegister(0x4B, R4B);
  BK4819_WriteRegister(0x7E, R7E);
}

uint8_t reg47values[] = {1, 7, 5};

static void SetModulation(ModulationType type) {
  RestoreRegisters();
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  reg &= ~(0b111 << 8);
  BK4819_WriteRegister(BK4819_REG_47, reg | (reg47values[type] << 8));
  if (type == MOD_USB) {
    BK4819_WriteRegister(0x3D, 0b0010101101000101);
    BK4819_WriteRegister(BK4819_REG_37, 0x160F);
    BK4819_WriteRegister(0x48, 0b0000001110101000);
    BK4819_WriteRegister(0x4B, R4B | (1 << 5));
  } else if (type == MOD_AM) {
    /* reg = BK4819_ReadRegister(0x7E);
    reg &= ~(0b111);
    reg |= 0b101;
    reg &= ~(0b111 << 12);
    reg |= 0b010 << 12;
    reg &= ~(1 << 15);
    reg |= 1 << 15;
    BK4819_WriteRegister(0x7E, reg); */
  }
}

static void ToggleAFDAC(bool on) {
  uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~(1 << 9);
  if (on)
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void ResetRSSI() {
  uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
  Reg |= 1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

uint32_t fMeasure = 0;
static void SetF(uint32_t f) {
  if (fMeasure == f) {
    return;
  }

  fMeasure = f;

  if (IsBroadcastFM(fMeasure)) {
    uint16_t f = fMeasure * 1e-4;
    BK1080_Init(f, true);
    BK1080_SetFrequency(f);
  } else {
    BK4819_PickRXFilterPathBasedOnFrequency(fMeasure);
    BK4819_SetFrequency(fMeasure);
    uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_30, reg);
  }
}

static void SetBW(BK4819_FilterBandwidth_t bw) {
  if (settings.bw == bw) {
    return;
  }
  BK4819_SetFilterBandwidth(bw = settings.bw);
}

// Spectrum related

bool IsPeakOverLevel() { return peak.rssi >= settings.rssiTriggerLevel; }

static void ResetRSSIHistory() {
  for (int i = 0; i < 128; ++i) {
    rssiHistory[i] = 0;
  }
}

static void ResetPeak() {
  peak.rssi = 0;
  peak.t = 0;
  peak.f = 0;
}

bool IsCenterMode() {
  return false; /*settings.scanStepIndex < S_STEP_2_5kHz;*/
}
uint8_t GetStepsCount() { return 128 >> settings.stepsCount; }
uint16_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }
uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }
uint32_t GetFStart() {
  return IsCenterMode() ? currentFreq - (GetBW() >> 1) : currentFreq;
}
uint32_t GetFEnd() { return currentFreq + GetBW(); }

static void TuneToPeak() { SetF(scanInfo.f = peak.f); }

static void DeInitSpectrum() {
  SetF(currentFreq);
  RestoreRegisters();
  isInitialized = false;
}

uint8_t GetBWIndex() {
  uint16_t step = GetScanStep();
  if (step < 1250) {
    return BK4819_FILTER_BW_NARROWER;
  } else if (step < 2500) {
    return BK4819_FILTER_BW_NARROW;
  } else {
    return BK4819_FILTER_BW_WIDE;
  }
}

uint8_t GetRssi() {
  ResetRSSI();
  SYSTICK_DelayUs(settings.scanDelay);
  return clamp(BK4819_GetRSSI(), 0, 255);
}

static bool audioState = true;
static void ToggleAudio(bool on) {
  if (on == audioState) {
    return;
  }
  audioState = on;
  if (on) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
}

static void ToggleRX(bool on) {
  if (isListening == on) {
    return;
  }
  isListening = on;

  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, on);
  ToggleAudio(on);

  if (on) {
    if (IsBroadcastFM(peak.f)) {
      BK1080_Mute(false);
    } else {
      SetBW(settings.listenBw);
  SetBW(settings.listenBw);
  ToggleAFDAC(true);
  ToggleAFBit(true);
    }
  } else {
    ToggleAFDAC(false);
    ToggleAFBit(false);
    BK1080_Mute(true);
    BK1080_Init(0, false);
  }
}

// Scan info

static void ResetScanStats() {
  scanInfo.rssi = 0;
  scanInfo.rssiMax = 0;
  scanInfo.iPeak = 0;
  scanInfo.fPeak = 0;
}

static void InitScan() {
  ResetScanStats();
  scanInfo.i = 0;
  scanInfo.f = GetFStart();

  scanInfo.scanStep = GetScanStep();
  scanInfo.measurementsCount = GetStepsCount();
}

static void ResetBlacklist() {
  for (int i = 0; i < 128; ++i) {
    if (rssiHistory[i] == 255)
      rssiHistory[i] = 0;
  }
}

static void NewBandOrLevel() {
  ResetPeak();
  InitScan();
  ToggleRX(false);
  scanInfo.rssiMin = 255;
  settings.rssiTriggerLevel = 255;
}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
    scanInfo.fPeak = scanInfo.f;
    scanInfo.iPeak = scanInfo.i;
  }

  if (scanInfo.rssi < scanInfo.rssiMin) {
    scanInfo.rssiMin = scanInfo.rssi;
  }
}

static void UpdatePeakInfoForce() {
  peak.t = 0;
  peak.rssi = scanInfo.rssiMax;
  peak.f = scanInfo.fPeak;
  peak.i = scanInfo.iPeak;
  if (settings.rssiTriggerLevel == 255) {
    settings.rssiTriggerLevel = clamp(scanInfo.rssiMax + 2, 0, 255);
  }
}

static void UpdatePeakInfo() {
  if (peak.f == 0 || peak.t >= 512 || peak.rssi < scanInfo.rssiMax)
    UpdatePeakInfoForce();
}

static void Measure() { rssiHistory[scanInfo.i] = scanInfo.rssi = GetRssi(); }

// Update things by keypress

static void UpdateRssiTriggerLevel(int diff) {
  settings.rssiTriggerLevel += diff;
}

static void UpdateScanStep(int diff) {
  if ((diff > 0 && settings.scanStepIndex < S_STEP_100_0kHz) ||
      (diff < 0 && settings.scanStepIndex > 0)) {
    settings.scanStepIndex += diff;
    SetBW(GetBWIndex());
    scanInfo.rssiMin = 255;
    settings.frequencyChangeStep = GetBW() >> 1;
  }
}

static void UpdateCurrentFreq(long int diff) {
  if ((diff > 0 && currentFreq < F_MAX) || (diff < 0 && currentFreq > F_MIN)) {
    currentFreq += diff;
  }
}

static void UpdateCurrentFreqStill(long int diff) {
  uint8_t offset = 50;
  switch (settings.modulationType) {
  case MOD_FM:
    offset = 100;
    break;
  case MOD_AM:
    offset = 50;
    break;
  case MOD_USB:
    offset = 10;
    break;
  }
  if ((offset > 0 && peak.f < F_MAX) || (offset < 0 && peak.f > F_MIN)) {
    peak.f += diff * offset;
  }
}

static void UpdateFreqChangeStep(long int diff) {
  settings.frequencyChangeStep =
      clamp(settings.frequencyChangeStep + diff, 10000, 200000);
}

static void ToggleModulation() {
  if (settings.modulationType < MOD_USB) {
    settings.modulationType++;
  } else {
    settings.modulationType = MOD_FM;
  }
}

static void ToggleBW() {
  if (settings.listenBw == BK4819_FILTER_BW_NARROWER) {
    settings.listenBw = BK4819_FILTER_BW_WIDE;
    return;
  }
  settings.listenBw++;
}

static void ToggleBacklight() {
  settings.backlightState = !settings.backlightState;
  if (settings.backlightState) {
    GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
  } else {
    GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
  }
}

static void ToggleStepsCount() {
  if (settings.stepsCount == STEPS_128) {
    settings.stepsCount = STEPS_16;
  } else {
    settings.stepsCount--;
  }
  settings.frequencyChangeStep = GetBW() >> 1;
}

char freqInputString[11] = "----------\0"; // XXXX.XXXXX\0
uint8_t freqInputDotIndex = 0;

static void ResetFreqInput() {
  tempFreq = 0;
  for (int i = 0; i < 10; ++i) {
    freqInputString[i] = '-';
  }
}

static void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
  SetState(FREQ_INPUT);
}

static void UpdateFreqInput(KEY_Code_t key) {
  if (key != KEY_EXIT && freqInputIndex >= 10) {
    return;
  }
  if (key == KEY_STAR) {
    freqInputDotIndex = freqInputIndex;
  }
  if (key == KEY_EXIT) {
    freqInputIndex--;
  } else {
    freqInputArr[freqInputIndex++] = key;
  }

  ResetFreqInput();

  uint8_t dotIndex =
      freqInputDotIndex == 0 ? freqInputIndex : freqInputDotIndex;

  KEY_Code_t digitKey;
  for (int i = 0; i < 10; ++i) {
    if (i < freqInputIndex) {
      digitKey = freqInputArr[i];
      freqInputString[i] = digitKey <= KEY_9 ? '0' + digitKey : '.';
    } else {
      freqInputString[i] = '-';
    }
  }

  uint32_t base = 100000; // 1MHz in BK units
  for (int i = dotIndex - 1; i >= 0; --i) {
    tempFreq += freqInputArr[i] * base;
    base *= 10;
  }

  base = 10000; // 0.1MHz in BK units
  if (dotIndex < freqInputIndex) {
    for (int i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += freqInputArr[i] * base;
      base /= 10;
    }
  }
  redrawScreen = true;
}

static void Blacklist() { rssiHistory[peak.i] = 255; }

// Draw things

uint8_t Rssi2Y(uint8_t rssi) {
  return DrawingEndY - clamp(rssi - scanInfo.rssiMin, 0, DrawingEndY);
}

static void DrawSpectrum() {
  for (uint8_t x = 0; x < 128; ++x) {
    uint8_t v = rssiHistory[x >> settings.stepsCount];
    if (v != 255) {
      DrawHLine(Rssi2Y(v), DrawingEndY, x, true);
    }
  }
}

static void DrawStatus() {
  char String[32];

  if (currentState == STILL) {
    sprintf(String, "%s %s", modulationTypeOptions[settings.modulationType],
            bwOptions[settings.listenBw]);
    GUI_DisplaySmallest(String, 1, 2, true, true);
    if (menuState != MENU_OFF) {
      sprintf(String, "%s:%u", menuItems[menuState],
              GetRegMenuValue(menuState));
      GUI_DisplaySmallest(String, 88, 2, true, true);
    }
  } else {
    sprintf(String, "%ux%u.%02uk %u.%ums %s %s", GetStepsCount(),
            GetScanStep() / 100, GetScanStep() % 100, settings.scanDelay / 1000,
            settings.scanDelay / 100 % 10,
            modulationTypeOptions[settings.modulationType],
            bwOptions[settings.listenBw]);
    GUI_DisplaySmallest(String, 1, 2, true, true);
  }
}

static void DrawCurrentF() {
  char String[16];

  sprintf(String, "%u.%05u", fMeasure / 100000, fMeasure % 100000);
  UI_PrintString(String, 0, 127, 0, 8, 1);
}

static void DrawNums() {
  char String[16];

  if (IsCenterMode()) {
    sprintf(String, "%u.%05u \xB1%u.%02uk", currentFreq / 100000,
            currentFreq % 100000, settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    GUI_DisplaySmallest(String, 36, 49, false, true);
  } else {
    sprintf(String, "%u.%05u", GetFStart() / 100000, GetFStart() % 100000);
    GUI_DisplaySmallest(String, 0, 49, false, true);

    sprintf(String, "\xB1%u.%02uk", settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    GUI_DisplaySmallest(String, 48, 49, false, true);

    sprintf(String, "%u.%05u", GetFEnd() / 100000, GetFEnd() % 100000);
    GUI_DisplaySmallest(String, 93, 49, false, true);
  }
}

static void DrawRssiTriggerLevel() {
  if (settings.rssiTriggerLevel == 255)
    return;
  uint8_t y = Rssi2Y(settings.rssiTriggerLevel);
  for (uint8_t x = 0; x < 128; x += 2) {
    PutPixel(x, y, true);
  }
}

static void DrawTicks() {
  uint32_t f = GetFStart() % 100000;
  uint32_t step = GetScanStep();
  for (uint8_t i = 0; i < 128; i += (1 << settings.stepsCount), f += step) {
    uint8_t barValue = 0b00000100;
    (f % 10000) < step && (barValue |= 0b00001000);
    (f % 50000) < step && (barValue |= 0b00010000);
    (f % 100000) < step && (barValue |= 0b01100000);

    gFrameBuffer[5][i] |= barValue;
  }

  // center
  /* if (IsCenterMode()) {
      gFrameBuffer[5][62] = 0x80;
      gFrameBuffer[5][63] = 0x80;
      gFrameBuffer[5][64] = 0xff;
      gFrameBuffer[5][65] = 0x80;
      gFrameBuffer[5][66] = 0x80;
  } else {
      gFrameBuffer[5][0] = 0xff;
      gFrameBuffer[5][1] = 0x80;
      gFrameBuffer[5][2] = 0x80;
      gFrameBuffer[5][3] = 0x80;
      gFrameBuffer[5][124] = 0x80;
      gFrameBuffer[5][125] = 0x80;
      gFrameBuffer[5][126] = 0x80;
      gFrameBuffer[5][127] = 0xff;
  } */
}

static void DrawArrow(uint8_t x) {
  for (signed i = -2; i <= 2; ++i) {
    signed v = x + i;
    if (!(v & 128)) {
      gFrameBuffer[5][v] |= (0b01111000 << my_abs(i)) & 0b01111000;
    }
  }
}

static void OnKeyDown(uint8_t key) {
  switch (key) {
  case KEY_1:
    if (settings.scanDelay < 8000) {
      settings.scanDelay += 100;
      NewBandOrLevel();
      preventKeypress = true;
    }
    break;
  case KEY_7:
    if (settings.scanDelay > 400) {
      settings.scanDelay -= 100;
      NewBandOrLevel();
      preventKeypress = true;
    }
    break;
  case KEY_3:
    UpdateScanStep(1);
    NewBandOrLevel();
    ResetBlacklist();
    break;
  case KEY_9:
    UpdateScanStep(-1);
    NewBandOrLevel();
    ResetBlacklist();
    break;
  case KEY_2:
    UpdateFreqChangeStep(GetScanStep() * 4);
    break;
  case KEY_8:
    UpdateFreqChangeStep(-GetScanStep() * 4);
    break;
  case KEY_UP:
    if (menuState != MENU_OFF) {
      SetRegMenuValue(menuState, true);
      break;
    }
    UpdateCurrentFreq(settings.frequencyChangeStep);
    NewBandOrLevel();
    ResetBlacklist();
    preventKeypress = true;
    break;
  case KEY_DOWN:
    if (menuState != MENU_OFF) {
      SetRegMenuValue(menuState, false);
      break;
    }
    UpdateCurrentFreq(-settings.frequencyChangeStep);
    NewBandOrLevel();
    ResetBlacklist();
    preventKeypress = true;
    break;
  case KEY_SIDE1:
    Blacklist();
    ResetPeak();
    preventKeypress = true;
    break;
  case KEY_STAR:
    UpdateRssiTriggerLevel(1);
    SYSTEM_DelayMs(90);
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(-1);
    SYSTEM_DelayMs(90);
    break;
  case KEY_5:
    FreqInput();
    break;
  case KEY_0:
    ToggleModulation();
    SetModulation(settings.modulationType);
    break;
  case KEY_6:
    ToggleBW();
    break;
  case KEY_4:
    ToggleStepsCount();
    NewBandOrLevel();
    ResetBlacklist();
    break;
  case KEY_SIDE2:
    ToggleBacklight();
    break;
  case KEY_PTT:
    SetState(STILL);
    break;
  case KEY_MENU:
    break;
  case KEY_EXIT:
    if (menuState != MENU_OFF) {
      menuState = MENU_OFF;
      break;
    }
    DeInitSpectrum();
    break;
  }
  redrawScreen = true;
}

static void OnKeyDownFreqInput(uint8_t key) {
  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
  case KEY_STAR:
    UpdateFreqInput(key);
    break;
  case KEY_EXIT:
    if (freqInputIndex == 0) {
      SetState(previousState);
      preventKeypress = true;
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU:
    if (tempFreq >= F_MIN && tempFreq <= F_MAX) {
      peak.f = currentFreq = tempFreq;
      NewBandOrLevel();
      ResetBlacklist();
      SetState(previousState);
      ResetRSSIHistory();
      preventKeypress = true;
    }
    break;
  }
}

void OnKeyDownStill(KEY_Code_t key) {
  switch (key) {
  case KEY_UP:
    if (menuState != MENU_OFF) {
      SetRegMenuValue(menuState, true);
      break;
    }
    UpdateCurrentFreqStill(1);
    TuneToPeak();
    break;
  case KEY_DOWN:
    if (menuState != MENU_OFF) {
      SetRegMenuValue(menuState, false);
      break;
    }
    UpdateCurrentFreqStill(-1);
    TuneToPeak();
    break;
  case KEY_STAR:
    UpdateRssiTriggerLevel(1);
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(-1);
    break;
  case KEY_5:
    FreqInput();
    break;
  case KEY_0:
    ToggleModulation();
    SetModulation(settings.modulationType);
    break;
  case KEY_6:
    ToggleBW();
    break;
  case KEY_SIDE2:
    ToggleBacklight();
    break;
  case KEY_PTT:
    // TODO: start transmit
    /* BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, false);
    BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_RED, true); */
    break;
  case KEY_MENU:
    if (menuState < MENU_AGCDC) {
      menuState++;
    } else {
      menuState = MENU_OFF;
    }
    break;
  case KEY_EXIT:
    if (menuState == MENU_OFF) {
      SetState(SPECTRUM);
      break;
    }
    NewBandOrLevel();
    menuState = MENU_OFF;
    preventKeypress = true;
    break;
  default:
    break;
  }
  redrawScreen = true;
}

static void RenderFreqInput() {
  UI_PrintString(freqInputString, 2, 127, 0, 8, true);
}

static void RenderStatus() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  DrawStatus();
  ST7565_BlitStatusLine();
}

static void RenderSpectrum() {
  DrawTicks();
  DrawArrow(peak.i << settings.stepsCount);
  DrawSpectrum();
  DrawRssiTriggerLevel();
  DrawCurrentF();
  DrawNums();
}

static void RenderStill() {
  DrawCurrentF();

  for (int i = 0; i < 128; i += 4) {
    gFrameBuffer[2][i] = 0b11000000;
  }

  for (int i = 0; i < (peak.rssi >> 1); ++i) {
    gFrameBuffer[2][i] |= 0b00011110;
  }

  gFrameBuffer[2][settings.rssiTriggerLevel >> 1] = 0b11111111;
}

static void Render() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  switch (currentState) {
  case SPECTRUM:
    RenderSpectrum();
    break;
  case FREQ_INPUT:
    RenderFreqInput();
    break;
  case STILL:
    RenderStill();
    break;
  }

  ST7565_BlitFullScreen();
}

bool HandleUserInput() {
  btnPrev = btn;
  btn = GetKey();

  if (btn == 255) {
    btnCounter = 0;
    return true;
  }

  if (btn == btnPrev && btnCounter < 255) {
    btnCounter++;
    SYSTEM_DelayMs(20);
  }

  if (btnPrev == 255 || btnCounter > 40) {
    switch (currentState) {
    case SPECTRUM:
      OnKeyDown(btn);
      break;
    case FREQ_INPUT:
      OnKeyDownFreqInput(btn);
      break;
    case STILL:
      OnKeyDownStill(btn);
      break;
    }
    RenderStatus();
  }

  return true;
}

static void Scan() {
  if (rssiHistory[scanInfo.i] != 255) {
    SetF(scanInfo.f);
    SetBW(GetBWIndex());
    Measure();
    UpdateScanInfo();
  }
}

static void NextScanStep() {
  ++peak.t;
  ++scanInfo.i;
  scanInfo.f += scanInfo.scanStep;
}

static bool ScanDone() { return scanInfo.i >= scanInfo.measurementsCount; }

static void Update() {
  // skip scanning while listening
  if (isListening && listenT) {
    listenT--;
    SYSTEM_DelayMs(1);
    return;
  }

  // prepare to check if peak over level while listening
  if (isListening) {
    ResetPeak();
    ResetScanStats();
    SetBW(GetBWIndex());
  }

  Scan();

  // reset to listening BW while we in listening mode
  if (isListening) {
    SetBW(settings.listenBw);
  }

  // check peak & (re)start listening mode if peak over level
  if (ScanDone() || isListening) {
    redrawScreen = true;
    preventKeypress = false;
    UpdatePeakInfo();
    TuneToPeak();
    if (IsPeakOverLevel()) {
      ToggleRX(true);
      listenT = 1000;
      return;
    }
    ToggleRX(false);
    InitScan();
    return;
  }

  NextScanStep();
}

static void UpdateStill() {
  // skip scanning while listening
  if (isListening && listenT) {
    listenT--;
    SYSTEM_DelayMs(1);
    return;
  }

  peak.rssi = 0;
  scanInfo.rssi = 0;
  scanInfo.rssiMax = 0;

  // prepare to check if peak over level while listening
  if (isListening) {
    SetBW(GetBWIndex());
    Scan();
    SetBW(settings.listenBw);
  } else {
    Scan();
  }

  redrawScreen = true;
  UpdatePeakInfoForce();

  if (IsPeakOverLevel()) {
    ToggleRX(true);
    listenT = 1000;
    return;
  }

  ToggleRX(false);
}

static void Tick() {
  if (!preventKeypress) {
    HandleUserInput();
  }
  switch (currentState) {
  case STILL:
    UpdateStill();
    break;
  case SPECTRUM:
    Update();
    break;
  case FREQ_INPUT:
    break;
  }
  if (redrawScreen) {
    Render();
    redrawScreen = false;
  }
}

void APP_RunSpectrum() {
  // TX here coz it always? set to active VFO
  currentFreq = gEeprom.VfoInfo[gEeprom.TX_CHANNEL].pRX->Frequency;

  BackupRegisters();

  // as in initial settings of spectrum
  BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE);

  NewBandOrLevel();
  ResetRSSIHistory();
  SetModulation(settings.modulationType = MOD_FM);
  // HACK: to make sure that all params are set to our default
  ToggleRX(true), ToggleRX(false);
  isInitialized = true;
  RenderStatus();
  while (isInitialized) {
    Tick();
  }
}

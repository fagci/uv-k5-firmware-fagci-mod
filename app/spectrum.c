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

#include "../app/spectrum.h"

extern void APP_RunSpectrum(void);

struct {
  uint32_t Frequency;
  uint32_t Offset;
} EEPROMFreqInfo;

enum State {
  SPECTRUM,
  FREQ_INPUT,
} currentState = SPECTRUM;

struct PeakInfo {
  uint8_t t;
  uint8_t rssi;
  uint8_t i;
  uint32_t f;
} peak;

struct SpectrumSettings {
  uint8_t mode;
  uint32_t frequencyChangeStep;
  uint16_t scanDelay;
  uint8_t rssiTriggerLevel;

  bool isStillMode;
  int32_t stillOffset;
} settings = {5, 80000, 800, 50, false, 0};

static const uint8_t DrawingEndY = 42;

static const uint8_t ModesCount = 7;
static const uint8_t LastLowBWModeIndex = 3;

static const uint32_t modeHalfSpectrumBW[] = {1600,  5000,  10000, 20000,
                                              40000, 80000, 160000};
static const uint16_t modeScanStep[] = {100, 312, 625, 1250, 2500, 2500, 2500};
static const uint8_t modeXdiv[] = {2, 2, 2, 2, 2, 1, 0};

uint8_t rssiHistory[128] = {};
uint32_t fMeasure;

uint8_t rssiMin = 255, rssiMax = 0;
uint8_t btnCounter = 0;

key_t btn;
uint8_t btnPrev;
uint32_t currentFreq, tempFreq;
uint8_t freqInputIndex = 0;
uint8_t freqInputArr[7] = {};
uint16_t oldAFSettings;
uint16_t oldBWSettings;

bool isAMOn = false;

bool isInitialized;
bool resetBlacklist;

// GUI functions

static void PutPixel(uint8_t x, uint8_t y) {
  gFrameBuffer[y >> 3][x] |= 1 << (y & 7);
}
static void PutPixelStatus(uint8_t x, uint8_t y) { gStatusLine[x] |= 1 << y; }

static void DrawHLine(int sy, int ey, int nx) {
  for (int i = sy; i <= ey; i++) {
    if (i < 56 && nx < 128) {
      PutPixel(nx, i);
    }
  }
}

static void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                bool statusbar) {
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
            PutPixelStatus(x + i, y + j);
          else
            PutPixel(x + i, y + j);
        }
        pixels >>= 1;
      }
    }
    x += 4;
  }
}

// Utility functions

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

static uint8_t my_abs(signed v) { return v > 0 ? v : -v; }

// Radio functions

static void RestoreOldAFSettings() {
  BK4819_WriteRegister(BK4819_REG_47, oldAFSettings);
}

static void ToggleAFBit(bool on) {
  uint16_t reg = BK4819_GetRegister(BK4819_REG_47);
  reg &= ~(1 << 8);
  if (on)
    reg |= 1 << 8;
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAM(bool on) {
  uint16_t reg = BK4819_GetRegister(BK4819_REG_47);
  reg &= ~(0b111 << 8);
  reg |= 0b1;
  if (on) {
    reg |= 0b111 << 8;
  }
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAFDAC(bool on) {
  uint32_t Reg = BK4819_GetRegister(BK4819_REG_30);
  Reg &= ~(1 << 9);
  if (on)
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

bool rxState = true;
static void ToggleRX(bool on) {
  if (rxState == on) {
    return;
  }
  rxState = on;
  BK4819_ToggleGpioOut(6, on);
  ToggleAFDAC(on);
  ToggleAFBit(on);
  if (on) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
}

uint32_t ReadFreqFromEEPROM(uint8_t channel) {
  uint8_t band = gEeprom.VfoInfo[channel].Band;
  uint32_t offset = 0xc80 + (band * 0x20) + (channel * 0x10);
  EEPROM_ReadBuffer(offset, &EEPROMFreqInfo, 8);
  return EEPROMFreqInfo.Frequency;
}

static void WriteFreqToEEPROM(uint32_t f, uint8_t channel) {
  uint8_t band = FREQUENCY_GetBand(f);
  gEeprom.VfoInfo[channel].Band = band;
  uint32_t offset = 0xc80 + (band * 0x20) + (channel * 0x10);
  EEPROMFreqInfo.Frequency = f;
  EEPROM_WriteBuffer(offset, &EEPROMFreqInfo);
}

static void SetBW() {
  uint16_t step = modeScanStep[settings.mode];
  if (step < 1250) {
    BK4819_WriteRegister(BK4819_REG_43, 0b0100000001011000);
  } else if (step < 2500) {
    BK4819_WriteRegister(BK4819_REG_43, 0x4048);
  } else {
    BK4819_WriteRegister(BK4819_REG_43, 0x3028);
  }
}

static void ResetRSSI() {
  uint32_t Reg = BK4819_GetRegister(BK4819_REG_30);
  Reg &= ~1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
  Reg |= 1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t f) {
  BK4819_PickRXFilterPathBasedOnFrequency(currentFreq);
  BK4819_SetFrequency(f);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, 0xBFF1);
}

uint8_t GetRssi() {
  ResetRSSI();

  SYSTICK_DelayUs(settings.scanDelay << (settings.mode <= LastLowBWModeIndex));
  uint16_t v = BK4819_GetRegister(0x67) & 0x1FF;
  return v < 255 ? v : 255;
}

// Spectrum related

static void ResetRSSIHistory() {
  for (int i = 0; i < 128; ++i) {
    rssiHistory[i] = 0;
  }
}
static void ResetPeak() { peak.rssi = 0; }

uint16_t GetScanStep() { return modeScanStep[settings.mode]; }
uint32_t GetBW() { return modeHalfSpectrumBW[settings.mode] << 1; }
uint32_t GetFStart() { return currentFreq - modeHalfSpectrumBW[settings.mode]; }
uint32_t GetFEnd() { return currentFreq + modeHalfSpectrumBW[settings.mode]; }
uint32_t GetPeakF() { return peak.f + settings.stillOffset; }
uint8_t GetMeasurementsCount() { return 128 >> modeXdiv[settings.mode]; }

uint8_t Rssi2Y(uint8_t rssi) {
  return DrawingEndY - clamp(rssi - rssiMin, 0, DrawingEndY);
}

// Update things by keypress

static void UpdateRssiTriggerLevel(int diff) { settings.rssiTriggerLevel += diff; }

static void UpdateBWMul(int diff) {
  if ((diff > 0 && settings.mode < (ModesCount - 1)) ||
      (diff < 0 && settings.mode > 0)) {
    settings.mode += diff;
    SetBW();
    rssiMin = 255;
    settings.frequencyChangeStep = modeHalfSpectrumBW[settings.mode];
  }
}

static void UpdateCurrentFreq(long int diff) {
  if (settings.isStillMode) {
    settings.stillOffset += diff > 0 ? 50 : -50;
    peak.i = (GetPeakF() - GetFStart()) / GetScanStep();
    ResetRSSIHistory();
    return;
  }
  if ((diff > 0 && currentFreq < 130000000) ||
      (diff < 0 && currentFreq > 1800000)) {
    currentFreq += diff;
  }
}

static void UpdateFreqChangeStep(long int diff) {
  settings.frequencyChangeStep =
      clamp(settings.frequencyChangeStep + diff, 10000, 200000);
}

static void Blacklist() { rssiHistory[peak.i] = 255; }

// Draw things

static void DrawSpectrum() {
  uint8_t div = modeXdiv[settings.mode];
  for (uint8_t x = 0; x < 128; ++x) {
    uint8_t v = rssiHistory[x >> div];
    if (v != 255) {
      DrawHLine(Rssi2Y(v), DrawingEndY, x);
    }
  }
}

static void DrawNums() {
  char String[32];

  sprintf(String, "%3.3f", GetPeakF() * 1e-5);
  UI_PrintString(String, 2, 127, 0, 8, 1);

  sprintf(String, isAMOn ? "AM" : "FM");
  GUI_DisplaySmallest(String, 0, 3, false);

  if (settings.isStillMode) {
    sprintf(String, "O: %2.1fkHz", settings.stillOffset * 1e-2);
    GUI_DisplaySmallest(String, 0, 9, false);
  }

  /* sprintf(String, "%3d", Rssi2dBm(rssiMax));
  GUI_DisplaySmallest(String, 112, 3);

  sprintf(String, "%3d", Rssi2dBm(rssiMin));
  GUI_DisplaySmallest(String, 112, 9);

  sprintf(String, "%3d", Rssi2dBm(settings.rssiTriggerLevel));
  GUI_DisplaySmallestStatus(String, 112, 2); */

  sprintf(String, "%1.2fM %2.2fk \xB1%3.0fk %1.1fms", GetBW() * 1e-5,
          GetScanStep() * 1e-2, settings.frequencyChangeStep * 1e-2,
          settings.scanDelay * 1e-3);
  GUI_DisplaySmallest(String, 1, 2, true);

  sprintf(String, "%04.1f", GetFStart() * 1e-5);
  GUI_DisplaySmallest(String, 0, 49, false);

  sprintf(String, "%04.1f", GetFEnd() * 1e-5);
  GUI_DisplaySmallest(String, 105, 49, false);
}

static void DrawRssiTriggerLevel() {
  uint8_t y = Rssi2Y(settings.rssiTriggerLevel);
  for (uint8_t x = 0; x < 126; x += 2) {
    PutPixel(x, y);
  }
}

static void DrawTicks() {
  /* uint32_t f = GetFStart() % 100000;
  uint32_t step = GetScanStep();
  for (uint8_t i = 0; i < 128; i += (1 << modeXdiv[settings.mode]), f += step) {
    uint8_t barValue = 0b00000100;
    (f % 10000) < step && (barValue |= 0b00001000);
    (f % 50000) < step && (barValue |= 0b00010000);
    (f % 100000) < step && (barValue |= 0b01100000);

    gFrameBuffer[5][i] |= barValue;
  } */

  // center
  gFrameBuffer[5][64] = 0b10101000;
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
      rssiMin = 255;
    }
    break;
  case KEY_7:
    if (settings.scanDelay > 400) {
      settings.scanDelay -= 100;
      rssiMin = 255;
    }
    break;
  case KEY_3:
    UpdateBWMul(1);
    resetBlacklist = true;
    break;
  case KEY_9:
    UpdateBWMul(-1);
    resetBlacklist = true;
    break;
  case KEY_2:
    UpdateFreqChangeStep(10000);
    break;
  case KEY_8:
    UpdateFreqChangeStep(-10000);
    break;
  case KEY_UP:
    UpdateCurrentFreq(settings.frequencyChangeStep);
    resetBlacklist = true;
    break;
  case KEY_DOWN:
    UpdateCurrentFreq(-settings.frequencyChangeStep);
    resetBlacklist = true;
    break;
  case KEY_0:
    Blacklist();
    break;
  case KEY_STAR:
    UpdateRssiTriggerLevel(1);
    SYSTEM_DelayMs(90);
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(-1);
    SYSTEM_DelayMs(90);
    break;
  case KEY_MENU:
    currentState = FREQ_INPUT;
    freqInputIndex = 0;
    break;
  case KEY_4:
    isAMOn = !isAMOn;
    ToggleAM(isAMOn);
    break;
  case KEY_6:
    settings.isStillMode = !settings.isStillMode;
    if (!settings.isStillMode) {
      settings.stillOffset = 0;
    } else {
      ResetRSSIHistory();
    }
    break;
  }
  ResetPeak();
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
    freqInputArr[freqInputIndex++] = key;
    break;
  case KEY_EXIT:
    if (freqInputIndex == 0) {
      currentState = SPECTRUM;
      break;
    }
    freqInputIndex--;
    break;
  case KEY_MENU:
    if (tempFreq >= 1800000 && tempFreq <= 130000000) {
      peak.f = currentFreq = tempFreq;
      settings.stillOffset = 0;
      WriteFreqToEEPROM(currentFreq, 0);
      resetBlacklist = true;
      currentState = SPECTRUM;
      peak.i = GetMeasurementsCount() >> 1;
      ResetRSSIHistory();
    }
    break;
  }
}

static void DeInitSpectrum() {
  SetF(currentFreq);
  RestoreOldAFSettings();
  BK4819_WriteRegister(0x43, oldBWSettings);
  ToggleRX(true);
  isInitialized = false;
}

static void Render() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  DrawTicks();
  DrawArrow(peak.i << modeXdiv[settings.mode]);
  DrawSpectrum();
  DrawRssiTriggerLevel();
  DrawNums();
  ST7565_BlitStatusLine();
  ST7565_BlitFullScreen();
}

bool HandleUserInput() {
  btnPrev = btn;
  btn = KEYBOARD_Poll();
  if (btn == KEY_EXIT) {
    DeInitSpectrum();
    return false;
  }

  if (btn == 255) {
    btnCounter = 0;
    return true;
  }

  if (btn == btnPrev && btnCounter < 255) {
    btnCounter++;
    SYSTEM_DelayMs(20);
  }
  if (btnPrev == 255 || btnCounter > 16) {
    switch (currentState) {
    case SPECTRUM:
      OnKeyDown(btn);
      Render();
      break;
    case FREQ_INPUT:
      OnKeyDownFreqInput(btn);
      break;
    }
  }

  return true;
}

static void Scan() {
  uint8_t rssi = 0;
  uint8_t iPeak = 0;
  uint32_t fPeak = currentFreq;

  rssiMax = 0;
  fMeasure = GetFStart();

  uint16_t scanStep = GetScanStep();
  uint8_t measurementsCount = GetMeasurementsCount();

  for (uint8_t i = 0;
       i < measurementsCount && (KEYBOARD_Poll() == 255 || resetBlacklist);
       ++i, fMeasure += scanStep) {
    if (!resetBlacklist && rssiHistory[i] == 255) {
      continue;
    }
    SetF(fMeasure);
    rssi = rssiHistory[i] = GetRssi();
    if (rssi > rssiMax) {
      rssiMax = rssi;
      fPeak = fMeasure;
      iPeak = i;
    }
    if (rssi < rssiMin) {
      rssiMin = rssi;
    }
  }
  resetBlacklist = false;
  ++peak.t;

  if (rssiMax > peak.rssi || peak.t >= 16) {
    peak.t = 0;
    peak.rssi = rssiMax;
    peak.f = fPeak;
    peak.i = iPeak;
  }
}

static void Listen() {
  if (fMeasure != GetPeakF()) {
    fMeasure = GetPeakF();
    SetF(fMeasure);
  }
  ToggleRX(peak.rssi >= settings.rssiTriggerLevel);
  BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE);
  for (uint8_t i = 0; i < 50 && KEYBOARD_Poll() == 255; ++i) {
    SYSTEM_DelayMs(20);
  }
  SetBW();
  peak.rssi = rssiHistory[peak.i] = GetRssi();
}

static void RenderFreqInput() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  tempFreq = 0;

  for (int i = 0; i < freqInputIndex; ++i) {
    tempFreq *= 10;
    tempFreq += freqInputArr[i];
  }
  tempFreq *= 100;

  char String[16];

  sprintf(String, "%4.3f", tempFreq * 1e-5);
  UI_PrintString(String, 2, 127, 0, 8, 1);

  ST7565_BlitFullScreen();
}

static void Update() {
  if (settings.isStillMode || peak.rssi >= settings.rssiTriggerLevel) {
    BACKLIGHT_TurnOn();
    Listen();
  }
  if (rssiMin == 255 ||
      (!settings.isStillMode && peak.rssi < settings.rssiTriggerLevel)) {
    ToggleRX(false);
    SetBW();
    Scan();
  }
}

static void Tick() {
  if (HandleUserInput()) {
    switch (currentState) {
    case SPECTRUM:
      Update();
      if (rssiMin != 255) {
        Render();
      }
      break;
    case FREQ_INPUT:
      RenderFreqInput();
      break;
    }
  }
}

void APP_RunSpectrum() {
  currentFreq = ReadFreqFromEEPROM(0);
  // BK4819_WriteRegister(BK4819_REG_48, 0xb3a8);
  // BK4819_SetAF(BK4819_AF_OPEN); // TODO: remove after calling spectrum from
  // FW
  oldAFSettings = BK4819_GetRegister(0x47);
  oldBWSettings = BK4819_GetRegister(0x43);
  SetBW();
  ResetPeak();
  resetBlacklist = true;
  ToggleRX(false);
  isInitialized = true;
  while (isInitialized) {
    Tick();
  }
}

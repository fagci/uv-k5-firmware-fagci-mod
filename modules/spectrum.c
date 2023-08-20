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

#include "../modules/spectrum.h"
#include "../modules/spectrum_gui.h"

enum State {
  SPECTRUM,
  FREQ_INPUT,
} currentState = SPECTRUM;

static const uint8_t DrawingEndY = 42;

static const uint8_t ModesCount = 7;
static const uint8_t LastLowBWModeIndex = 3;

static const uint32_t modeHalfSpectrumBW[] = {1600,  5000,  10000, 20000,
                                              40000, 80000, 160000};
static const uint16_t modeScanStep[] = {100, 312, 625, 1250, 2500, 2500, 2500};
static const uint8_t modeXdiv[] = {2, 2, 2, 2, 2, 1, 0};

uint8_t rssiHistory[128] = {};
uint32_t fMeasure;

uint8_t peakT = 0;
uint8_t peakRssi = 0;
uint8_t peakI = 0;
uint32_t peakF = 0;
uint8_t rssiMin = 255;
uint8_t btnCounter = 0;

uint16_t scanDelay = 800;
uint8_t mode = 5;
uint8_t rssiTriggerLevel = 50;

key_t btn;
uint8_t btnPrev;
uint32_t currentFreq, tempFreq;
uint8_t freqInputIndex = 0;
uint8_t freqInputArr[7] = {};
uint16_t oldAFSettings;
uint16_t oldBWSettings;
uint32_t frequencyChangeStep = 80000;

bool isInitialized;
bool resetBlacklist;

void ResetPeak() { peakRssi = 0; }

void SetBW() { BK4819_SetFilterBandwidth(mode <= LastLowBWModeIndex); }
void RestoreOldAFSettings() { BK4819_WriteRegister(0x47, oldAFSettings); }
void ToggleAFDAC(bool on) {
  uint32_t Reg = BK4819_GetRegister(BK4819_REG_30);
  Reg &= ~(1 << 9);
  if (on)
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}
uint16_t GetScanStep() { return modeScanStep[mode]; }
uint32_t GetBW() { return modeHalfSpectrumBW[mode] << 1; }
uint32_t GetFStart() { return currentFreq - modeHalfSpectrumBW[mode]; }
uint32_t GetFEnd() { return currentFreq + modeHalfSpectrumBW[mode]; }

uint8_t GetMeasurementsCount() { return 128 >> modeXdiv[mode]; }

void ResetRSSI() {
  uint32_t Reg = BK4819_GetRegister(BK4819_REG_30);
  Reg &= ~1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
  Reg |= 1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

void SetF(uint32_t f) {
  BK4819_SetFrequency(f);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, 0xBFF1);
}

uint8_t GetRssi() {
  ResetRSSI();

  SYSTICK_DelayUs(scanDelay << (mode <= LastLowBWModeIndex));
  uint16_t v = BK4819_GetRegister(0x67) & 0x1FF;
  return v < 255 ? v : 255;
}

void ToggleGreen(bool flag) { BK4819_ToggleGpioOut(6, flag); }

int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

uint8_t Rssi2Y(uint8_t rssi) {
  return DrawingEndY - clamp(rssi - rssiMin, 0, DrawingEndY);
}

void DrawSpectrum() {
  for (uint8_t x = 0; x < 128; ++x) {
    uint8_t v = rssiHistory[x >> modeXdiv[mode]];
    if (v != 255) {
      DrawHLine(Rssi2Y(v), DrawingEndY, x);
    }
  }
}

void DrawNums() {
  char String[16];

  sprintf(String, "%3.3f", peakF * 1e-5);
  GUI_PrintString(String, 2, 127, 0, 8, 1);

  sprintf(String, "%1.2fM \xB1%3.0fk %2.0f", GetBW() * 1e-5,
          frequencyChangeStep * 1e-2, scanDelay * 1e-2);
  GUI_DisplaySmallestStatus(16, String, 1, 2);

  sprintf(String, "%04.1f", GetFStart() * 1e-5);
  GUI_DisplaySmallest(7, String, 0, 49);

  sprintf(String, "%04.1f", GetFEnd() * 1e-5);
  GUI_DisplaySmallest(7, String, 105, 49);
}

void DrawRssiTriggerLevel() {
  uint8_t y = Rssi2Y(rssiTriggerLevel);
  for (uint8_t x = 0; x < 126; x += 4) {
    DrawLine(x, x + 2, y);
  }
}

void DrawTicks() {
  uint32_t f = GetFStart() % 100000;
  uint32_t step = GetScanStep();
  for (uint8_t i = 0; i < 128; i += (1 << modeXdiv[mode]), f += step) {
    uint8_t barValue = 0b00000100;
    (f % 10000) < step && (barValue |= 0b00001000);
    (f % 50000) < step && (barValue |= 0b00010000);
    (f % 100000) < step && (barValue |= 0b01100000);

    gFrameBuffer[5][i] |= barValue;
  }

  // center
  gFrameBuffer[5][64] = 0b10101000;
}

uint8_t my_abs(signed v) { return v > 0 ? v : -v; }

void DrawArrow(uint8_t x) {
  for (signed i = -2; i <= 2; ++i) {
    signed v = x + i;
    if (!(v & 128)) {
      gFrameBuffer[5][v] |= (0b01111000 << my_abs(i)) & 0b01111000;
    }
  }
}

void UpdateRssiTriggerLevel(int diff) { rssiTriggerLevel += diff; }

void UpdateBWMul(int diff) {
  if ((diff > 0 && mode < (ModesCount - 1)) || (diff < 0 && mode > 0)) {
    mode += diff;
    SetBW();
    rssiMin = 255;
    frequencyChangeStep = modeHalfSpectrumBW[mode];
  }
}

void UpdateCurrentFreq(long int diff) {
  if ((diff > 0 && currentFreq < 130000000) ||
      (diff < 0 && currentFreq > 1800000)) {
    currentFreq += diff;
  }
}

void UpdateFreqChangeStep(long int diff) {
  frequencyChangeStep = clamp(frequencyChangeStep + diff, 10000, 200000);
}

void Blacklist() { rssiHistory[peakI] = 255; }

void OnKeyDown(uint8_t key) {
  switch (key) {
  case KEY_1:
    if (scanDelay < 8000) {
      scanDelay += 100;
      rssiMin = 255;
    }
    break;
  case KEY_7:
    if (scanDelay > 400) {
      scanDelay -= 100;
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
    UpdateCurrentFreq(frequencyChangeStep);
    resetBlacklist = true;
    break;
  case KEY_DOWN:
    UpdateCurrentFreq(-frequencyChangeStep);
    resetBlacklist = true;
    break;
  case KEY_5:
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
  }
  ResetPeak();
}

void OnKeyDownFreqInput(uint8_t key) {
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
    if (freqInputIndex > 0) {
      freqInputIndex--;
    } else {
      currentState = SPECTRUM;
    }
    break;
  case KEY_MENU:
    currentFreq = tempFreq;
    currentState = SPECTRUM;
    break;
  }
}

void InitSpectrum() {
  currentFreq = 43400000;
  oldAFSettings = BK4819_GetRegister(0x47);
  oldBWSettings = BK4819_GetRegister(0x43);
  SetBW();
  ResetPeak();
  resetBlacklist = true;
  ToggleGreen(false);
  isInitialized = true;
}

void DeInitSpectrum() {
  SetF(currentFreq);
  RestoreOldAFSettings();
  BK4819_WriteRegister(0x43, oldBWSettings);
  ToggleGreen(true);
  isInitialized = false;
}

void Render() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  DrawTicks();
  DrawArrow(peakI << modeXdiv[mode]);
  DrawSpectrum();
  DrawRssiTriggerLevel();
  DrawNums();
  ST7565_BlitStatusLine();
  ST7565_BlitFullScreen();
}

bool HandleUserInput() {
  btnPrev = btn;
  btn = KEYBOARD_Poll();
  /* if (btn == KEY_EXIT) {
    DeInitSpectrum();
    return false;
  } */

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

void Scan() {
  uint8_t rssi = 0, rssiMax = 0;
  uint8_t iPeak = 0;
  uint32_t fPeak = currentFreq;

  fMeasure = GetFStart();

  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  BK4819_SetAF(BK4819_AF_MUTE);
  ToggleAFDAC(false);

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
  ++peakT;

  if (rssiMax > peakRssi || peakT >= 16) {
    peakT = 0;
    peakRssi = rssiMax;
    peakF = fPeak;
    peakI = iPeak;
  }
}

void Listen() {
  if (fMeasure != peakF) {
    fMeasure = peakF;
    SetF(fMeasure);
    RestoreOldAFSettings();
    BK4819_SetAF(BK4819_AF_OPEN);
    ToggleAFDAC(true);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
  for (uint8_t i = 0; i < 16 && KEYBOARD_Poll() == 255; ++i) {
    SYSTEM_DelayMs(64);
  }
  peakRssi = rssiHistory[peakI] = GetRssi();
}

void RenderFreqInput() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  tempFreq = 0;

  for (int i = 0; i < freqInputIndex; ++i) {
    tempFreq *= 10;
    tempFreq += freqInputArr[i];
  }
  tempFreq *= 100;

  char String[16];

  sprintf(String, "%4.3f", tempFreq * 1e-5);
  GUI_PrintString(String, 2, 127, 0, 8, 1);

  ST7565_BlitStatusLine();
  ST7565_BlitFullScreen();
}

void Update() {
  if (peakRssi >= rssiTriggerLevel) {
    ToggleGreen(true);
    BACKLIGHT_TurnOn();
    Listen();
  }
  if (peakRssi < rssiTriggerLevel) {
    ToggleGreen(false);
    Scan();
  }
}

void HandleSpectrum() {
  if (!isInitialized) {
    InitSpectrum();
  }

  if (isInitialized && HandleUserInput()) {
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

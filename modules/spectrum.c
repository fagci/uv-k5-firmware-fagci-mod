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

static const uint8_t DrawingEndY = 42;
// static const uint16_t BarPos = 5 * 128;

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
uint32_t currentFreq;
uint16_t oldAFSettings;
uint16_t oldBWSettings;
uint32_t frequencyChangeStep = 40000;

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

void PutPixel(uint8_t x, uint8_t y) { gFrameBuffer[y >> 3][x] |= 1 << (y % 8); }

void DrawHLine(int sy, int ey, int nx) {
  for (int i = sy; i <= ey; i++) {
    if (i < 56 && nx < 128) {
      PutPixel(nx, i);
    }
  }
}

void DrawLine(int sx, int ex, int ny) {
  for (int i = sx; i <= ex; i++) {
    if (i < 128 && ny < 56) {
      PutPixel(i, ny);
    }
  }
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
  /* Display.SetCoursorXY(0, 0);
  Display.PrintFixedDigitsNumber3(scanDelay, 2, 2, 1);

  Display.SetCoursorXY(105, 0);
  Display.PrintFixedDigitsNumber3(GetBW(), 3, 3, 2);

  Display.SetCoursorXY(42, 0);
  Display.PrintFixedDigitsNumber3(peakF, 2, 6, 3);

  Display.SetCoursorXY(0, 48);
  Display.PrintFixedDigitsNumber3(GetFStart(), 4, 4, 1);

  Display.SetCoursorXY(98, 48);
  Display.PrintFixedDigitsNumber3(GetFEnd(), 4, 4, 1);

  Display.SetCoursorXY(52, 48);
  Display.PrintFixedDigitsNumber3(frequencyChangeStep, 3, 3, 2); */
}

void DrawRssiTriggerLevel() {
  uint8_t y = Rssi2Y(rssiTriggerLevel);
  for (uint8_t x = 0; x < 126; x += 4) {
    DrawLine(x, x + 2, y);
  }
}

void DrawTicks() {
  // center
  gFrameBuffer[5][64] = 0b00111000;
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
  }
  ResetPeak();
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

bool HandleUserInput() {
  btnPrev = btn;
  btn = KEYBOARD_Poll();
  if (btn == KEY_EXIT) {
    DeInitSpectrum();
    return false;
  }

  if (btn != 255) {
    if (btn == btnPrev && btnCounter < 255) {
      btnCounter++;
    }
    if (btnPrev == 255 || btnCounter > 16) {
      OnKeyDown(btn);
    }
    return true;
  }

  btnCounter = 0;
  return true;
}

void Render() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  DrawTicks();
  DrawArrow(peakI << modeXdiv[mode]);
  DrawSpectrum();
  DrawRssiTriggerLevel();
  DrawNums();
  ST7565_BlitFullScreen();
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

void Update() {
  if (peakRssi >= rssiTriggerLevel) {
    ToggleGreen(true);
    BACKLIGHT_TurnOn();
    // GPIOC->DATA |= GPIO_PIN_4;
    Listen();
  }
  if (peakRssi < rssiTriggerLevel) {
    ToggleGreen(false);
    // GPIOC->DATA &= ~GPIO_PIN_4;
    Scan();
  }
}

void HandleSpectrum() {
  if (!isInitialized) {
    InitSpectrum();
  }

  if (isInitialized && HandleUserInput()) {
    Update();
    Render();
  }
}

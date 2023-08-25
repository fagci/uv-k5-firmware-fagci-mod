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
#include <stdint.h>
#include <stdio.h>
#include "driver/eeprom.h"
#include "settings.h"

enum State {
  SPECTRUM,
  FREQ_INPUT,
  REGISTERS,
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
uint8_t rssiMin = 255, rssiMax = 0;
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

bool isAMOn = false;

bool isInitialized;
bool resetBlacklist;

void ResetPeak() { peakRssi = 0; }

void SetBW() {
  uint16_t step = modeScanStep[mode];
  if (step < 1250) {
    BK4819_WriteRegister(BK4819_REG_43, 0b0100000001011000);
  } else if (step < 2500) {
    BK4819_WriteRegister(BK4819_REG_43, 0x4048);
  } else {
    BK4819_WriteRegister(BK4819_REG_43, 0x3028);
  }
}
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

int Rssi2dBm(uint8_t rssi) { return (rssi >> 1) - 160; }

uint8_t Rssi2Y(uint8_t rssi) {
  return DrawingEndY - clamp(rssi - rssiMin, 0, DrawingEndY);
}

void DrawSpectrum() {
  uint8_t div = modeXdiv[mode];
  for (uint8_t x = 0; x < 128; ++x) {
    uint8_t v = rssiHistory[x >> div];
    if (v != 255) {
      DrawHLine(Rssi2Y(v), DrawingEndY, x);
    }
  }
}

void DrawNums() {
  char String[32];

  sprintf(String, "%3.3f", peakF * 1e-5);
  GUI_PrintString(String, 2, 127, 0, 8, 1);

  sprintf(String, isAMOn ? "AM" : "FM");
  GUI_DisplaySmallest(3, String, 0, 3);

  sprintf(String, "%3d", Rssi2dBm(rssiMax));
  GUI_DisplaySmallest(5, String, 112, 3);

  sprintf(String, "%3d", Rssi2dBm(rssiMin));
  GUI_DisplaySmallest(5, String, 112, 9);

  sprintf(String, "%3d", Rssi2dBm(rssiTriggerLevel));
  GUI_DisplaySmallestStatus(5, String, 112, 2);

  sprintf(String, "%1.2fM %2.2fk \xB1%3.0fk %1.1fms", GetBW() * 1e-5,
          GetScanStep() * 1e-2, frequencyChangeStep * 1e-2, scanDelay * 1e-3);
  GUI_DisplaySmallestStatus(32, String, 1, 2);

  sprintf(String, "%04.1f", GetFStart() * 1e-5);
  GUI_DisplaySmallest(7, String, 0, 49);

  sprintf(String, "%04.1f", GetFEnd() * 1e-5);
  GUI_DisplaySmallest(7, String, 105, 49);
}

void DrawRssiTriggerLevel() {
  uint8_t y = Rssi2Y(rssiTriggerLevel);
  for (uint8_t x = 0; x < 126; x += 2) {
    PutPixel(x, y);
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

void SaveFrequency(uint32_t f) {
  // EEPROM_WriteBuffer writes 8 bytes at a time; take account of this
  // by reading 8 bytes in and only replacing 4 of them.
  uint8_t VFOBuffer[8];
  EEPROM_ReadBuffer(0xc80, VFOBuffer, 8);
  // Flip endianness of currentFreq before writing it
  uint32_t flippedFreq =
      ((f & 0xff000000) >> 24) | ((f & 0x00ff0000) >> 8) |
      ((f & 0x0000ff00) << 8) | ((f & 0x000000ff) << 24);
  memcpy(VFOBuffer, (uint8_t *) &flippedFreq, 4);
  EEPROM_WriteBuffer(0xc80, VFOBuffer);
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
  case KEY_SIDE1:
    currentState = REGISTERS;
    SetF(currentFreq);
    BK4819_SetAF(isAMOn ? 0x7 : BK4819_AF_OPEN);
    BK4819_WriteRegister(BK4819_REG_48, 0xb3a8);
    ToggleAFDAC(true);
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
    break;
  case KEY_4:
    isAMOn = !isAMOn;
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
    if (freqInputIndex == 0) {
      currentState = SPECTRUM;
      break;
    }
    freqInputIndex--;
    break;
  case KEY_MENU:
    if (tempFreq >= 1800000 && tempFreq <= 130000000) {
      currentFreq = tempFreq;
      SaveFrequency(currentFreq);
      currentState = SPECTRUM;
    }
    break;
  }
}
struct RegisterValue {
  const char *name;
  unsigned value;
};

struct RegisterField {
  const char *name;
  uint8_t from;
  uint8_t to;
};

struct RegisterName {
  const char *name;
  uint8_t number;
  const struct RegisterField *fields;
  uint8_t fieldsCount;
};

const struct RegisterField reg0x43[] = {
    {"Gain after FM dem", 2, 2},
    {"BW mode", 4, 5},
    {"RF filter BW", 12, 14},
    {"RF filter BW weak", 9, 11},
};

const struct RegisterField reg0x47[] = {
    {"txFilterBypassAll", 0, 0},
    {"afOutput", 8, 11},
    {"afOutputInverse", 13, 13},
};

const struct RegisterField reg0x3D[] = {
    {"IF Selection", 0, 15},
};

const struct RegisterField reg0x30[] = {
    {"VCO Calibration Enable", 15, 15},
    {"Rx Link Enable (LNA/MIXER/PGA/ADC)", 10, 13},
    {"AF DAC Enable", 9, 9},
    {"DISC Mode Disable", 8, 8},
    {"PLL/VCO Enable", 4, 7},
    {"PA Gain Enable", 3, 3},
    {"MIC ADC Enable", 2, 2},
    {"Tx DSP Enable", 1, 1},
    {"Rx DSP Enable", 0, 0},
};

const struct RegisterField reg0x37[] = {
    {"ANA LDO Bypass", 7, 7},  {"VCO LDO Bypass", 6, 6},
    {"RF LDO Bypass", 5, 5},   {"PLL LDO Bypass", 4, 4},
    {"DSP Enable", 2, 2},      {"XTAL Enable", 1, 1},
    {"Band-Gap Enable", 0, 0},
};

const struct RegisterField reg0x2B[] = {
    {"Disable AFRxHPF300filter", 10, 10},
    {"Disable AF RxLPF3K filter", 9, 9},
    {"Disable AF Rx de-emphasisfilter", 8, 8},
};

const struct RegisterField reg0x28[] = {
    {"Expander AF Rx Ratio.", 14, 15},
    {"Expander AF Rx 0 dB point(dB)", 7, 13},
    {"Expander AF Rx noise point(dB)", 0, 6},
};

const struct RegisterField reg0x10[] = {
    {"AGC table", 0, 15},
};

const struct RegisterField reg0x11[] = {
    {"AGC table", 0, 15},
};

const struct RegisterField reg0x12[] = {
    {"AGC table", 0, 15},
};

const struct RegisterField reg0x13[] = {
    {"AGC table", 0, 15},
};

const struct RegisterField reg0x14[] = {
    {"AGC table", 0, 15},
};

const struct RegisterField reg0x54[] = {
    {"300Hz AF Response coef", 0, 15},
};

const struct RegisterField reg0x55[] = {
    {"300Hz AF Response coef", 0, 15},
};

const struct RegisterField reg0x48[] = {
    {"AF Rx Gain1", 10, 11},
    {"AF Rx Gain2", 4, 9},
    {"AF DAC Gain (after Gain1 and Gain2)", 0, 3},
    {"AF Level Contr(ALC) Disable", 5, 5},
};

const struct RegisterField reg0x73[] = {
    {"AFC Range Selection.", 11, 13},
    {"AFC Disable", 4, 4},
};

const struct RegisterField reg0x7E[] = {
    {"AGC Fix Mode.", 15, 15},
    {"AGC Fix Index.", 12, 14},
    {"DC Filter BW for Rx (IF In).", 0, 2},
};

const struct RegisterName registersMenu[] = {
    {"AGC table 10", 0x10, reg0x10, 1},
    {"AGC table 11", 0x11, reg0x11, 1},
    {"AGC table 12", 0x12, reg0x12, 1},
    {"AGC table 13", 0x13, reg0x13, 1},
    {"AGC table 14", 0x14, reg0x14, 1},
    {"AF Expander 28", 0x28, reg0x28, 3},
    {"AF filters 2B", 0x2B, reg0x2B, 3},
    {"PLL/VCO 30", 0x30, reg0x30, 9},
    {"REG 37", 0x37, reg0x37, 7},
    {"REG 3D", 0x3D, reg0x3D, 1},
    {"REG 43", 0x43, reg0x43, 4},
    {"AF 47", 0x47, reg0x47, 3},
    {"AF 48", 0x48, reg0x48, 4},
    {"300Hz resp coef 54", 0x54, reg0x54, 1},
    {"300Hz resp coef 55", 0x55, reg0x55, 1},
    {"Auto F correction 73", 0x73, reg0x73, 2},
    {"AGC/DC 7E", 0x7E, reg0x7E, 3},
};

uint8_t registersMenuIndex = 0;
uint8_t registerIndex = 0;
uint8_t menuLevel = 0;

uint16_t GetRegFieldMaxValue(struct RegisterField regField) {
  return (1 << ((regField.to - regField.from) + 1)) - 1;
}

void ToggleRegister(bool next) {
  struct RegisterName reg = registersMenu[registersMenuIndex];
  struct RegisterField regField = reg.fields[registerIndex];
  uint16_t maxValue = GetRegFieldMaxValue(regField);
  uint16_t regValue = BK4819_GetRegister(reg.number);
  uint16_t fieldValue = (regValue >> regField.from) & maxValue;
  if (next && fieldValue < maxValue) {
    fieldValue++;
  }
  if (!next && fieldValue > 0) {
    fieldValue--;
  }
  regValue &= ~(maxValue << regField.from);
  BK4819_WriteRegister(reg.number, regValue | (fieldValue << regField.from));
}

void OnKeyDownRegisters(uint8_t key) {
  switch (key) {
  case KEY_4:
    ToggleRegister(false);
    break;
  case KEY_6:
    ToggleRegister(true);
    break;
  case KEY_UP:
    if (menuLevel == 0 && registersMenuIndex > 0) {
      registersMenuIndex--;
    }
    if (menuLevel == 1 && registerIndex > 0) {
      registerIndex--;
    }
    break;
  case KEY_DOWN:
    if (menuLevel == 0 &&
        registersMenuIndex <
            sizeof(registersMenu) / sizeof(struct RegisterName) - 1) {
      registersMenuIndex++;
    }
    if (menuLevel == 1 &&
        registerIndex < registersMenu[registersMenuIndex].fieldsCount - 1) {
      registerIndex++;
    }
    break;
  case KEY_MENU:
    if (menuLevel < 1) {
      menuLevel++;
    }
    break;
  case KEY_EXIT:
    if (menuLevel > 0) {
      menuLevel--;
      registerIndex = 0;
      break;
    }
    currentState = SPECTRUM;
    break;
  }
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
    case REGISTERS:
      OnKeyDownRegisters(btn);
      break;
    }
  }

  return true;
}

void Scan() {
  uint8_t rssi = 0;
  uint8_t iPeak = 0;
  uint32_t fPeak = currentFreq;

  rssiMax = 0;
  fMeasure = GetFStart();

  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  BK4819_PickRXFilterPathBasedOnFrequency(currentFreq);
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
    BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE);
    // RestoreOldAFSettings();
    BK4819_PickRXFilterPathBasedOnFrequency(currentFreq);
    BK4819_SetAF(isAMOn ? 0x7 : BK4819_AF_OPEN);
    BK4819_WriteRegister(BK4819_REG_48, 0xb3a8);
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

void ToBinary(char *output, uint16_t value, uint8_t size) {
  for (int i = 0; i < size; i++) {
    output[i] = value & 1 ? '1' : '0';
    value >>= 1;
  }
  output[size] = '\0';
}

void RenderRegistersMenu() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  char String[32];
  struct RegisterName reg = registersMenu[registersMenuIndex];

  if (menuLevel == 0) {
    int offsetY = registersMenuIndex > 6 ? -(registersMenuIndex - 6) : 0;
    for (int i = 0; i < sizeof(registersMenu) / sizeof(struct RegisterName);
         i++) {
      sprintf(String, i == registersMenuIndex ? "> %s" : "  %s",
              registersMenu[i].name);
      uint8_t y = (i + offsetY) * 6;
      if (y > DrawingEndY)
        continue;
      GUI_DisplaySmallest(32, String, 2, y);
    }
  } else {
    int offsetY = registerIndex > 4 ? -(registerIndex - 4) : 0;
    sprintf(String, reg.name);
    GUI_DisplaySmallestStatus(32, String, 2, 2);

    for (int i = 0; i < reg.fieldsCount; i++) {
      sprintf(String, i == registerIndex ? "> %s" : "  %s", reg.fields[i].name);
      uint8_t y = (i + offsetY) * 6;
      if (y > DrawingEndY - 12)
        continue;
      GUI_DisplaySmallest(32, String, 2, y);
    }
    struct RegisterField regField = reg.fields[registerIndex];

    uint16_t maxValue = GetRegFieldMaxValue(regField);
    uint16_t regValue = BK4819_GetRegister(reg.number);
    uint32_t v = (regValue >> regField.from) & maxValue;

    sprintf(String, "VALUE(4/6):");
    GUI_DisplaySmallest(16, String, 2, 40);

    ToBinary(String, v, regField.to - regField.from + 1);
    GUI_DisplaySmallest(16, String, 48, 40);

    /* sprintf(String, "#%x, max: %d, sz: %d", reg.number, maxValue,
            sizeof(&reg.fields));
    GUI_DisplaySmallest(32, String, 2, 46); */
  }

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
    SetBW();
    Scan();
  }
}

void Tick() {
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
    case REGISTERS:
      RenderRegistersMenu();
      break;
    }
  }
}

void InitSpectrum() {
  // Read EEPROM 0x0c80 into currentFreq
  EEPROM_ReadBuffer(0xc80, (uint8_t *)&currentFreq, 4);
  // Flip endianness in currentFreq
  currentFreq = ((currentFreq & 0xff000000) >> 24) |
                ((currentFreq & 0x00ff0000) >> 8) |
                ((currentFreq & 0x0000ff00) << 8) |
                ((currentFreq & 0x000000ff) << 24);
  oldAFSettings = BK4819_GetRegister(0x47);
  oldBWSettings = BK4819_GetRegister(0x43);
  SetBW();
  ResetPeak();
  resetBlacklist = true;
  ToggleGreen(false);
  isInitialized = true;
  while (isInitialized) {
    Tick();
  }
}

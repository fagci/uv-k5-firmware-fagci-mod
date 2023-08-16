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

uint16_t aGlyphs3x4[] = {
    // Hex    [1] [2] [3] [4] Scanlines
    0x0000, // 0x20
    0x4400, // 0x21 ! 010 010 000
    0xAA00, // 0x22 " 101 101 000
    0xAEE0, // 0x23 # 101 111 111
    0x64C4, // 0x24 $ 011 010 110 010
    0xCE60, // 0x25 % 110 111 011
    0x4C60, // 0x26 & 010 110 011
    0x4000, // 0x27 ' 010 000 000
    0x4840, // 0x28 ( 010 100 010
    0x4240, // 0x29 ) 010 001 010
    0x6600, // 0x2A * 011 011 000
    0x4E40, // 0x2B + 010 111 010
    0x0088, // 0x2C , 000 000 100 100
    0x0E00, // 0x2D - 000 111 000
    0x0080, // 0x2E . 000 000 100
    0x2480, // 0x2F / 001 010 100

    0x4A40, // 0x30 0 010 101 010
    0x8440, // 0x31 1 100 010 010
    0xC460, // 0x32 2 110 010 011
    0xE6E0, // 0x33 3 111 011 111
    0xAE20, // 0x34 4 101 111 001
    0x64C0, // 0x35 5 011 010 110
    0x8EE0, // 0x36 6 100 111 111
    0xE220, // 0x37 7 111 001 001
    0x6EC0, // 0x38 8 011 111 110
    0xEE20, // 0x39 9 111 111 001
    0x4040, // 0x3A : 010 000 010
    0x0448, // 0x3B ; 000 010 010 100
    0x4840, // 0x3C < 010 100 010
    0xE0E0, // 0x3D = 111 000 111
    0x4240, // 0x3E > 010 001 010
    0x6240, // 0x3F ? 011 001 010

    0xCC20, // 0x40 @ 110 110 001 // 0 = 000_
    0x4EA0, // 0x41 A 010 111 101 // 2 = 001_
    0xCEE0, // 0x42 B 110 111 111 // 4 = 010_
    0x6860, // 0x43 C 011 100 011 // 6 = 011_
    0xCAC0, // 0x44 D 110 101 110 // 8 = 100_
    0xECE0, // 0x45 E 111 110 111 // A = 101_
    0xEC80, // 0x46 F 111 110 100 // C = 110_
    0xCAE0, // 0x47 G 110 101 111 // E = 111_
    0xAEA0, // 0x48 H 101 111 101
    0x4440, // 0x49 I 010 010 010
    0x22C0, // 0x4A J 001 001 110
    0xACA0, // 0x4B K 101 110 101
    0x88E0, // 0x4C L 100 100 111
    0xEEA0, // 0x4D M 111 111 101
    0xEAA0, // 0x4E N 111 101 101
    0xEAE0, // 0x4F O 111 101 111

    0xEE80, // 0x50 P 111 111 100
    0xEAC0, // 0x51 Q 111 101 110
    0xCEA0, // 0x52 R 110 111 101
    0x64C0, // 0x53 S 011 010 110
    0xE440, // 0x54 T 111 010 010
    0xAAE0, // 0x55 U 101 101 111
    0xAA40, // 0x56 V 101 101 010
    0xAEE0, // 0x57 W 101 111 111
    0xA4A0, // 0x58 X 101 010 101
    0xA440, // 0x59 Y 101 010 010
    0xE4E0, // 0x5A Z 111 010 111
    0xC8C0, // 0x5B [ 110 100 110
    0x8420, // 0x5C \ 100 010 001
    0x6260, // 0x5D ] 011 001 011
    0x4A00, // 0x5E ^ 010 101 000
    0x00E0, // 0x5F _ 000 000 111

    0x8400, // 0x60 ` 100 010 000
    0x04C0, // 0x61 a 000 010 110
    0x8CC0, // 0x62 b 100 110 110
    0x0CC0, // 0x63 c 000 110 110
    0x4CC0, // 0x64 d 010 110 110
    0x08C0, // 0x65 e 000 100 110
    0x4880, // 0x66 f 010 100 100
    0x0CCC, // 0x67 g 000 110 110 110
    0x8CC0, // 0x68 h 100 110 110
    0x0880, // 0x69 i 000 100 100
    0x0448, // 0x6A j 000 010 010 100
    0x8CA0, // 0x6B k 100 110 101
    0x8840, // 0x6C l 100 100 010
    0x0CE0, // 0x6D m 000 110 111
    0x0CA0, // 0x6E n 000 110 101
    0x0CC0, // 0x6F o 000 110 110

    0x0CC8, // 0x70 p 000 110 110 100
    0x0CC4, // 0x71 q 000 110 110 010
    0x0C80, // 0x72 r 000 110 100
    0x0480, // 0x73 s 000 010 100
    0x4C60, // 0x74 t 010 110 011
    0x0AE0, // 0x75 u 000 101 111
    0x0A40, // 0x76 v 000 101 010
    0x0E60, // 0x77 w 000 111 011
    0x0CC0, // 0x78 x 000 110 110
    0x0AE2, // 0x79 y 000 101 111 001
    0x0840, // 0x7A z 000 100 010
    0x6C60, // 0x7B { 011 110 011
    0x4444, // 0x7C | 010 010 010 010
    0xC6C0, // 0x7D } 110 011 110
    0x6C00, // 0x7E ~ 011 110 000
    0xA4A4  // 0x7F   101 010 101 010 // Alternative: Could even have a "full"
            // 4x4 checkerboard
};

uint16_t SwapNibblesBitReverse(uint16_t x) {
  const uint8_t aReverseNibble[16] = {
      0x0, // 0  0000 -> 0000
      0x8, // 1  0001 -> 1000
      0x4, // 2  0010 -> 0100
      0xC, // 3  0011 -> 1100
      0x2, // 4  0100 -> 0010
      0xA, // 5  0101 -> 1010
      0x6, // 6  0110 -> 0110
      0xE, // 7  0111 -> 1110
      0x1, // 8  1000 -> 0001
      0x9, // 9  1001 -> 1001
      0x5, // A  1010 -> 0101
      0xD, // B  1011 -> 1101
      0x3, // C  1100 -> 0011
      0xB, // D  1101 -> 1011
      0x7, // E  1110 -> 0111
      0xF  // F  1111 -> 1111
  };

  return 0 | (aReverseNibble[(x >> 12) & 0xF] << 0) |
         (aReverseNibble[(x >> 8) & 0xF] << 4) |
         (aReverseNibble[(x >> 4) & 0xF] << 8) |
         (aReverseNibble[(x >> 0) & 0xF] << 12);
}

void GUI_DisplaySmallest(uint8_t Size, const char *pString, uint8_t x,
                         uint8_t y) {
  uint8_t i;
  uint8_t c;
  const uint8_t *p = (const uint8_t *)pString;

  for (i = 0; i < Size; i++) {
    c = *p++;
    uint16_t pixels = SwapNibblesBitReverse(aGlyphs3x4[c - 0x20]);
    for (int iy = 0; iy < 4; ++iy) {
      for (int ix = 0; ix < 4; ++ix) {
        bool px = ~((pixels & 1) - 1);
        if (px) {
          PutPixel(x + ix + (i * 4), y + iy);
        }
        pixels >>= 1;
      }
    }
  }
}

void DrawNums() {
  char String[7];
  sprintf(String, "%1.1f", scanDelay / 1000.0);
  GUI_DisplaySmallest(7, String, 0, 0);

  sprintf(String, "%3.1f", peakF / 100000.0);
  GUI_DisplaySmallest(7, String, 42, 0);

  sprintf(String, "%1.2f", GetBW() / 100000.0);
  GUI_DisplaySmallest(7, String, 105, 0);

  sprintf(String, "%3.1f", GetFStart() / 100000.0);
  GUI_DisplaySmallest(7, String, 0, 48);

  sprintf(String, "%3.1f", GetFEnd() / 100000.0);
  GUI_DisplaySmallest(7, String, 98, 48);

  sprintf(String, "%1.2f", frequencyChangeStep / 100000.0);
  GUI_DisplaySmallest(7, String, 52, 48);

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

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

void PutPixel(uint8_t x, uint8_t y) { gFrameBuffer[y >> 3][x] |= 1 << (y % 8); }
void PutPixelStatus(uint8_t x, uint8_t y) { gStatusLine[x] |= 1 << (y % 8); }

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

static const unsigned char font_3x5[] = {
    0x00, 0x00, 0x00, //  32 - space
    0x00, 0x17, 0x00, //  33 - exclam
    0x03, 0x00, 0x03, //  34 - quotedbl
    0x1f, 0x0a, 0x1f, //  35 - numbersign
    0x0a, 0x1f, 0x05, //  36 - dollar
    0x09, 0x04, 0x12, //  37 - percent
    0x0f, 0x17, 0x1c, //  38 - ampersand
    0x00, 0x03, 0x00, //  39 - quotesingle
    0x00, 0x0e, 0x11, //  40 - parenleft
    0x11, 0x0e, 0x00, //  41 - parenright
    0x05, 0x02, 0x05, //  42 - asterisk
    0x04, 0x0e, 0x04, //  43 - plus
    0x10, 0x08, 0x00, //  44 - comma
    0x04, 0x04, 0x04, //  45 - hyphen
    0x00, 0x10, 0x00, //  46 - period
    0x18, 0x04, 0x03, //  47 - slash
    0x1e, 0x11, 0x0f, //  48 - zero
    0x02, 0x1f, 0x00, //  49 - one
    0x19, 0x15, 0x12, //  50 - two
    0x11, 0x15, 0x0a, //  51 - three
    0x07, 0x04, 0x1f, //  52 - four
    0x17, 0x15, 0x09, //  53 - five
    0x1e, 0x15, 0x1d, //  54 - six
    0x19, 0x05, 0x03, //  55 - seven
    0x1f, 0x15, 0x1f, //  56 - eight
    0x17, 0x15, 0x0f, //  57 - nine
    0x00, 0x0a, 0x00, //  58 - colon
    0x10, 0x0a, 0x00, //  59 - semicolon
    0x04, 0x0a, 0x11, //  60 - less
    0x0a, 0x0a, 0x0a, //  61 - equal
    0x11, 0x0a, 0x04, //  62 - greater
    0x01, 0x15, 0x03, //  63 - question
    0x0e, 0x15, 0x16, //  64 - at
    0x1e, 0x05, 0x1e, //  65 - A
    0x1f, 0x15, 0x0a, //  66 - B
    0x0e, 0x11, 0x11, //  67 - C
    0x1f, 0x11, 0x0e, //  68 - D
    0x1f, 0x15, 0x15, //  69 - E
    0x1f, 0x05, 0x05, //  70 - F
    0x0e, 0x15, 0x1d, //  71 - G
    0x1f, 0x04, 0x1f, //  72 - H
    0x11, 0x1f, 0x11, //  73 - I
    0x08, 0x10, 0x0f, //  74 - J
    0x1f, 0x04, 0x1b, //  75 - K
    0x1f, 0x10, 0x10, //  76 - L
    0x1f, 0x06, 0x1f, //  77 - M
    0x1f, 0x0e, 0x1f, //  78 - N
    0x0e, 0x11, 0x0e, //  79 - O
    0x1f, 0x05, 0x02, //  80 - P
    0x0e, 0x19, 0x1e, //  81 - Q
    0x1f, 0x0d, 0x16, //  82 - R
    0x12, 0x15, 0x09, //  83 - S
    0x01, 0x1f, 0x01, //  84 - T
    0x0f, 0x10, 0x1f, //  85 - U
    0x07, 0x18, 0x07, //  86 - V
    0x1f, 0x0c, 0x1f, //  87 - W
    0x1b, 0x04, 0x1b, //  88 - X
    0x03, 0x1c, 0x03, //  89 - Y
    0x19, 0x15, 0x13, //  90 - Z
    0x1f, 0x11, 0x11, //  91 - bracketleft
    0x02, 0x04, 0x08, //  92 - backslash
    0x11, 0x11, 0x1f, //  93 - bracketright
    0x02, 0x01, 0x02, //  94 - asciicircum
    0x10, 0x10, 0x10, //  95 - underscore
    0x01, 0x02, 0x00, //  96 - grave
    0x1a, 0x16, 0x1c, //  97 - a
    0x1f, 0x12, 0x0c, //  98 - b
    0x0c, 0x12, 0x12, //  99 - c
    0x0c, 0x12, 0x1f, // 100 - d
    0x0c, 0x1a, 0x16, // 101 - e
    0x04, 0x1e, 0x05, // 102 - f
    0x0c, 0x2a, 0x1e, // 103 - g
    0x1f, 0x02, 0x1c, // 104 - h
    0x00, 0x1d, 0x00, // 105 - i
    0x10, 0x20, 0x1d, // 106 - j
    0x1f, 0x0c, 0x12, // 107 - k
    0x11, 0x1f, 0x10, // 108 - l
    0x1e, 0x0e, 0x1e, // 109 - m
    0x1e, 0x02, 0x1c, // 110 - n
    0x0c, 0x12, 0x0c, // 111 - o
    0x3e, 0x12, 0x0c, // 112 - p
    0x0c, 0x12, 0x3e, // 113 - q
    0x1c, 0x02, 0x02, // 114 - r
    0x14, 0x1e, 0x0a, // 115 - s
    0x02, 0x1f, 0x12, // 116 - t
    0x0e, 0x10, 0x1e, // 117 - u
    0x0e, 0x18, 0x0e, // 118 - v
    0x1e, 0x1c, 0x1e, // 119 - w
    0x12, 0x0c, 0x12, // 120 - x
    0x06, 0x28, 0x1e, // 121 - y
    0x1a, 0x1e, 0x16, // 122 - z
    0x04, 0x1b, 0x11, // 123 - braceleft
    0x00, 0x1b, 0x00, // 124 - bar
    0x11, 0x1b, 0x04, // 125 - braceright
    0x02, 0x03, 0x01, // 126 - asciitilde
    0x00, 0x00, 0x00, // 127 - empty
    0x00, 0x00, 0x00, // 128 - empty
    0x00, 0x00, 0x00, // 129 - empty
    0x00, 0x00, 0x00, // 130 - empty
    0x00, 0x00, 0x00, // 131 - empty
    0x00, 0x00, 0x00, // 132 - empty
    0x00, 0x00, 0x00, // 133 - empty
    0x00, 0x00, 0x00, // 134 - empty
    0x00, 0x00, 0x00, // 135 - empty
    0x00, 0x00, 0x00, // 136 - empty
    0x00, 0x00, 0x00, // 137 - empty
    0x00, 0x00, 0x00, // 138 - empty
    0x00, 0x00, 0x00, // 139 - empty
    0x00, 0x00, 0x00, // 140 - empty
    0x00, 0x00, 0x00, // 141 - empty
    0x00, 0x00, 0x00, // 142 - empty
    0x00, 0x00, 0x00, // 143 - empty
    0x00, 0x00, 0x00, // 144 - empty
    0x00, 0x00, 0x00, // 145 - empty
    0x00, 0x00, 0x00, // 146 - empty
    0x00, 0x00, 0x00, // 147 - empty
    0x00, 0x00, 0x00, // 148 - empty
    0x00, 0x00, 0x00, // 149 - empty
    0x00, 0x00, 0x00, // 150 - empty
    0x00, 0x00, 0x00, // 151 - empty
    0x00, 0x00, 0x00, // 152 - empty
    0x00, 0x00, 0x00, // 153 - empty
    0x00, 0x00, 0x00, // 154 - empty
    0x00, 0x00, 0x00, // 155 - empty
    0x00, 0x00, 0x00, // 156 - empty
    0x00, 0x00, 0x00, // 157 - empty
    0x00, 0x00, 0x00, // 158 - empty
    0x00, 0x00, 0x00, // 159 - empty
    0x00, 0x00, 0x00, // 160 - empty
    0x00, 0x1d, 0x00, // 161 - exclamdown
    0x0e, 0x1b, 0x0a, // 162 - cent
    0x14, 0x1f, 0x15, // 163 - sterling
    0x15, 0x0e, 0x15, // 164 - currency
    0x0b, 0x1c, 0x0b, // 165 - yen
    0x00, 0x1b, 0x00, // 166 - brokenbar
    0x14, 0x1b, 0x05, // 167 - section
    0x01, 0x00, 0x01, // 168 - dieresis
    0x02, 0x05, 0x05, // 169 - copyright
    0x16, 0x15, 0x17, // 170 - ordfeminine
    0x02, 0x05, 0x00, // 171 - guillemotleft
    0x02, 0x02, 0x06, // 172 - logicalnot
    0x04, 0x04, 0x00, // 173 - softhyphen
    0x07, 0x03, 0x04, // 174 - registered
    0x01, 0x01, 0x01, // 175 - macron
    0x02, 0x05, 0x02, // 176 - degree
    0x12, 0x17, 0x12, // 177 - plusminus
    0x01, 0x07, 0x04, // 178 - twosuperior
    0x05, 0x07, 0x07, // 179 - threesuperior
    0x00, 0x02, 0x01, // 180 - acute
    0x1f, 0x08, 0x07, // 181 - mu
    0x02, 0x1d, 0x1f, // 182 - paragraph
    0x0e, 0x0e, 0x0e, // 183 - periodcentered
    0x10, 0x14, 0x08, // 184 - cedilla
    0x00, 0x07, 0x00, // 185 - onesuperior
    0x12, 0x15, 0x12, // 186 - ordmasculine
    0x00, 0x05, 0x02, // 187 - guillemotright
    0x03, 0x08, 0x18, // 188 - onequarter
    0x0b, 0x18, 0x10, // 189 - onehalf
    0x03, 0x0b, 0x18, // 190 - threequarters
    0x18, 0x15, 0x10, // 191 - questiondown
    0x18, 0x0d, 0x1a, // 192 - Agrave
    0x1a, 0x0d, 0x18, // 193 - Aacute
    0x19, 0x0d, 0x19, // 194 - Acircumflex
    0x1a, 0x0f, 0x19, // 195 - Atilde
    0x1d, 0x0a, 0x1d, // 196 - Adieresis
    0x1f, 0x0b, 0x1c, // 197 - Aring
    0x1e, 0x1f, 0x15, // 198 - AE
    0x06, 0x29, 0x19, // 199 - Ccedilla
    0x1c, 0x1d, 0x16, // 200 - Egrave
    0x1e, 0x1d, 0x14, // 201 - Eacute
    0x1d, 0x1d, 0x15, // 202 - Ecircumflex
    0x1d, 0x1c, 0x15, // 203 - Edieresis
    0x14, 0x1d, 0x16, // 204 - Igrave
    0x16, 0x1d, 0x14, // 205 - Iacute
    0x15, 0x1d, 0x15, // 206 - Icircumflex
    0x15, 0x1c, 0x15, // 207 - Idieresis
    0x1f, 0x15, 0x0e, // 208 - Eth
    0x1d, 0x0b, 0x1e, // 209 - Ntilde
    0x1c, 0x15, 0x1e, // 210 - Ograve
    0x1e, 0x15, 0x1c, // 211 - Oacute
    0x1d, 0x15, 0x1d, // 212 - Ocircumflex
    0x1d, 0x17, 0x1e, // 213 - Otilde
    0x1d, 0x14, 0x1d, // 214 - Odieresis
    0x0a, 0x04, 0x0a, // 215 - multiply
    0x1e, 0x15, 0x0f, // 216 - Oslash
    0x1d, 0x12, 0x1c, // 217 - Ugrave
    0x1c, 0x12, 0x1d, // 218 - Uacute
    0x1d, 0x11, 0x1d, // 219 - Ucircumflex
    0x1d, 0x10, 0x1d, // 220 - Udieresis
    0x0c, 0x1a, 0x0d, // 221 - Yacute
    0x1f, 0x0a, 0x0e, // 222 - Thorn
    0x3e, 0x15, 0x0b, // 223 - germandbls
    0x18, 0x15, 0x1e, // 224 - agrave
    0x1a, 0x15, 0x1c, // 225 - aacute
    0x19, 0x15, 0x1d, // 226 - acircumflex
    0x1a, 0x17, 0x1d, // 227 - atilde
    0x19, 0x14, 0x1d, // 228 - adieresis
    0x18, 0x17, 0x1f, // 229 - aring
    0x1c, 0x1e, 0x0e, // 230 - ae
    0x04, 0x2a, 0x1a, // 231 - ccedilla
    0x08, 0x1d, 0x1e, // 232 - egrave
    0x0a, 0x1d, 0x1c, // 233 - eacute
    0x09, 0x1d, 0x1d, // 234 - ecircumflex
    0x09, 0x1c, 0x1d, // 235 - edieresis
    0x00, 0x1d, 0x02, // 236 - igrave
    0x02, 0x1d, 0x00, // 237 - iacute
    0x01, 0x1d, 0x01, // 238 - icircumflex
    0x01, 0x1c, 0x01, // 239 - idieresis
    0x0a, 0x17, 0x1d, // 240 - eth
    0x1d, 0x07, 0x1a, // 241 - ntilde
    0x08, 0x15, 0x0a, // 242 - ograve
    0x0a, 0x15, 0x08, // 243 - oacute
    0x09, 0x15, 0x09, // 244 - ocircumflex
    0x09, 0x17, 0x0a, // 245 - otilde
    0x09, 0x14, 0x09, // 246 - odieresis
    0x04, 0x15, 0x04, // 247 - divide
    0x1c, 0x16, 0x0e, // 248 - oslash
    0x0d, 0x12, 0x1c, // 249 - ugrave
    0x0c, 0x12, 0x1d, // 250 - uacute
    0x0d, 0x11, 0x1d, // 251 - ucircumflex
    0x0d, 0x10, 0x1d, // 252 - udieresis
    0x04, 0x2a, 0x1d, // 253 - yacute
    0x3e, 0x14, 0x08, // 254 - thorn
    0x05, 0x28, 0x1d  // 255 - ydieresis
};

void GUI_DisplaySmallest(int n, const char *pString, uint8_t x, uint8_t y) {
  uint8_t c;
  uint8_t pixels;
  const uint8_t *p = (const uint8_t *)pString;

  while (--n > 0 && (c = *p++)) {
    for (int i = 0; i < 3; ++i) {
      pixels = font_3x5[(c - 0x20) * 3 + i];
      for (int j = 0; j < 6; ++j) {
        if (pixels & 1) {
          PutPixel(x + i, y + j);
        }
        pixels >>= 1;
      }
    }
    x += 4;
  }
}

void GUI_DisplaySmallestStatus(int n, const char *pString, uint8_t x,
                               uint8_t y) {
  uint8_t c;
  uint8_t pixels;
  const uint8_t *p = (const uint8_t *)pString;

  while (--n > 0 && (c = *p++)) {
    for (int i = 0; i < 3; ++i) {
      pixels = font_3x5[(c - 0x20) * 3 + i];
      for (int j = 0; j < 6; ++j) {
        if (pixels & 1) {
          PutPixelStatus(x + i, y + j);
        }
        pixels >>= 1;
      }
    }
    x += 4;
  }
}

void DrawNums() {
  char String[16];

  sprintf(String, "%3.3f", peakF / 100000.0);
  GUI_PrintString(String, 2, 127, 0, 8, 1);

  sprintf(String, "%1.2fM \xB1%dk %2d", GetBW() / 100000.0,
          frequencyChangeStep / 100, scanDelay / 100);
  GUI_DisplaySmallestStatus(16, String, 1, 1);

  sprintf(String, "%3.1f", GetFStart() / 100000.0);
  GUI_DisplaySmallest(7, String, 0, 49);

  sprintf(String, "%3.1f", GetFEnd() / 100000.0);
  GUI_DisplaySmallest(7, String, 109, 49);
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
  for (uint8_t i = 0; i < 128; ++i, f += step) {
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

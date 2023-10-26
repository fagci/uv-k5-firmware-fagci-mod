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

#include "helper.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../misc.h"
#include "inputbox.h"
#include <stdint.h>
#include <string.h>

void UI_GenerateChannelString(char *pString, uint8_t Channel) {
  uint8_t i;

  if (gInputBoxIndex == 0) {
    sprintf(pString, "CH-%02d", Channel + 1);
    return;
  }

  pString[0] = 'C';
  pString[1] = 'H';
  pString[2] = '-';

  for (i = 0; i < 2; i++) {
    if (gInputBox[i] == 10) {
      pString[i + 3] = '-';
    } else {
      pString[i + 3] = gInputBox[i] + '0';
    }
  }
}

void UI_GenerateChannelStringEx(char *pString, bool bShowPrefix,
                                uint8_t ChannelNumber) {
  if (gInputBoxIndex) {
    uint8_t i;

    for (i = 0; i < 3; i++) {
      if (gInputBox[i] == 10) {
        pString[i] = '-';
      } else {
        pString[i] = gInputBox[i] + '0';
      }
    }
    return;
  }

  if (bShowPrefix) {
    sprintf(pString, "CH-%03d", ChannelNumber + 1);
  } else {
    if (ChannelNumber == 0xFF) {
      strcpy(pString, "NULL");
    } else {
      sprintf(pString, "%03d", ChannelNumber + 1);
    }
  }
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End,
                    uint8_t Line, uint8_t Width, bool bCentered) {
  uint32_t i, Length;

  Length = strlen(pString);
  if (bCentered) {
    Start += (((End - Start) - (Length * Width)) + 1) / 2;
  }
  for (i = 0; i < Length; i++) {
    if (pString[i] >= ' ') {
      uint8_t Index = pString[i] - ' ';
      uint8_t offset = (i * Width) + Start;
      memcpy(gFrameBuffer[Line + 0] + offset, &gFontBig[Index][0], 8);
      memcpy(gFrameBuffer[Line + 1] + offset, &gFontBig[Index][8], 8);
    }
  }
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End,
                         uint8_t Line) {
  const size_t Length = strlen(pString);
  size_t i;

  if (End > Start)
    Start += (((End - Start) - (Length * 8)) + 1) / 2;

  const unsigned int char_width = ARRAY_SIZE(gFontSmall[0]);
  const unsigned int char_spacing = char_width + 1;
  uint8_t *pFb = gFrameBuffer[Line] + Start;
  for (i = 0; i < Length; i++) {
    if (pString[i] >= 32) {
      const unsigned int Index = (unsigned int)pString[i] - 32;
      if (Index < ARRAY_SIZE(gFontSmall))
        memmove(pFb + (i * char_spacing), &gFontSmall[Index], char_width);
    }
  }
}

#if 1
void UI_DisplayFrequency(const char *pDigits, uint8_t X, uint8_t Y,
                         bool bDisplayLeadingZero, bool flag) {
  const unsigned int charWidth = 13;
  uint8_t *pFb0 = gFrameBuffer[Y] + X;
  uint8_t *pFb1 = pFb0 + 128;
  bool bCanDisplay = false;
  unsigned int i = 0;

  // MHz
  while (i < 3) {
    const unsigned int Digit = pDigits[i++];
    if (bDisplayLeadingZero || bCanDisplay || Digit > 0) {
      bCanDisplay = true;
      memmove(pFb0, gFontBigDigits[Digit], charWidth);
      memmove(pFb1, gFontBigDigits[Digit] + charWidth, charWidth);
    } else if (flag) {
      pFb0 -= 6;
      pFb1 -= 6;
    }
    pFb0 += charWidth;
    pFb1 += charWidth;
  }

  // decimal point
  *pFb1 = 0x60;
  pFb0++;
  pFb1++;
  *pFb1 = 0x60;
  pFb0++;
  pFb1++;
  *pFb1 = 0x60;
  pFb0++;
  pFb1++;

  // kHz
  while (i < 6) {
    const unsigned int Digit = pDigits[i++];
    memmove(pFb0, gFontBigDigits[Digit], charWidth);
    memmove(pFb1, gFontBigDigits[Digit] + charWidth, charWidth);
    pFb0 += charWidth;
    pFb1 += charWidth;
  }
}
#else
void UI_DisplayFrequency(const char *pDigits, uint8_t X, uint8_t Y,
                         bool bDisplayLeadingZero, bool bFlag) {
  char String[8];
  char baseDigit = '0';
  uint8_t d;
  sprintf(String, "---.---");
  for (int i = 0; i < 3; i++) {
    d = pDigits[i];
    if (d < 10) {
      String[i] = d + baseDigit;
    }
  }
  for (int i = 0; i < 3; i++) {
    d = pDigits[i + 3];
    if (d < 10) {
      String[i + 4] = d + baseDigit;
    }
  }
  UI_PrintString(String, 8, 127, Y, 8, 1);
}
#endif

void UI_DisplaySmallDigits(uint8_t Size, const char *pString, uint8_t X,
                           uint8_t Y) {
  uint8_t i;

  for (i = 0; i < Size; i++) {
    memcpy(gFrameBuffer[Y] + (i * 7) + X, gFontSmallDigits[(uint8_t)pString[i]],
           7);
  }
}

void PutPixel(uint8_t x, uint8_t y, uint8_t fill) {
  if (fill == 1) {
    gFrameBuffer[y >> 3][x] |= 1 << (y & 7);
  } else if (fill == 2) {
    gFrameBuffer[y >> 3][x] ^= 1 << (y & 7);
  } else {
    gFrameBuffer[y >> 3][x] &= ~(1 << (y & 7));
  }
}

void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
  if (fill) {
    gStatusLine[x] |= 1 << y;
  } else {
    gStatusLine[x] &= ~(1 << y);
  }
}

void DrawHLine(int sy, int ey, int nx, bool fill) {
  for (int i = sy; i <= ey; i++) {
    if (i < 56 && nx < LCD_WIDTH) {
      PutPixel(nx, i, fill);
    }
  }
}

void UI_PrintStringSmallest(const char *pString, uint8_t x, uint8_t y,
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

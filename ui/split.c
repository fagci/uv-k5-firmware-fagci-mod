#include "split.h"

void UI_DisplaySplit(void) {
  for (uint8_t line = 4; line < 7; line++) {
    memset(gFrameBuffer[line], 1, LCD_WIDTH);
  }
  ST7565_BlitFullScreen();
}

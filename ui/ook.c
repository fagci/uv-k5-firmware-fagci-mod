#include "ook.h"
#include "helper.h"

void UI_DisplayOOK() {
  for (uint8_t line = 4; line < 7; line++) {
    memset(gFrameBuffer[line], 0, LCD_WIDTH);
  }

  UI_PrintStringSmallest("OOK app", 0, 32, false, true);
  UI_PrintStringSmallest("Mode: bruteforce", 0, 40, false, true);
  UI_PrintStringSmallest("Range: 0..4096", 0, 46, false, true);

  ST7565_BlitFullScreen();
}

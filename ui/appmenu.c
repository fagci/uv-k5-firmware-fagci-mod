#include "appmenu.h"

void UI_DisplayAppMenu() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  UI_PrintStringSmallest("App menu", 0, 1, false, true);

  UI_PrintStringSmallest("1. Split", 0, 10, false, true);
  UI_PrintStringSmallest("2. OOK", 0, 16, false, true);

  ST7565_BlitFullScreen();
}

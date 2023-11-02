#include "ook.h"
#include "helper.h"

void UI_DisplayOOK() {
  UI_ClearAppScreen();

  UI_PrintStringSmallest("OOK app", 0, 32, false, true);
  UI_PrintStringSmallest("Mode: bruteforce", 0, 40, false, true);
  UI_PrintStringSmallest("Range: 0..4096", 0, 46, false, true);

  ST7565_BlitFullScreen();
}

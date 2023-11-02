#include "ook.h"

uint16_t counter;

void OOK_update() { counter++; }

void OOK_render() {
  char String[16];
  UI_ClearAppScreen();

  UI_PrintStringSmallest("OOK app", 0, 32, false, true);
  UI_PrintStringSmallest("Mode: bruteforce", 0, 40, false, true);

  sprintf(String, "%u", counter);
  UI_PrintStringSmallest(String, 0, 46, false, true);

  ST7565_BlitFullScreen();
}

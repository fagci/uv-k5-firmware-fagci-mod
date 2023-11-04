#include "appmenu.h"
#include "ui.h"

void UI_DisplayAppMenu() {
  char String[16];
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  UI_PrintStringSmallest("App menu", 0, 1, false, true);

  for (uint8_t i = 1; i < ARRAY_SIZE(appsNames); ++i) {
    sprintf(String, "%u: %s", i, appsNames[i]);
    UI_PrintStringSmallest(String, 0, 10 + i * 6, false, true);
  }

  ST7565_BlitFullScreen();
}

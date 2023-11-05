#include "appmenu.h"
#include "helper.h"
#include "ui.h"

void UI_DisplayAppMenu() {
  char String[16];
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  UI_PrintStringSmallBold("Apps", 0, 0, 0);

  for (uint8_t i = 1; i < ARRAY_SIZE(apps); ++i) {
    sprintf(String, "%u: %s", i, apps[i].name);
    UI_PrintStringSmall(String, 0, 0, i);
  }

  ST7565_BlitFullScreen();
}

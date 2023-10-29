#include "contextmenu.h"

void UI_DisplayContextMenu() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  UI_PrintStringSmallBold("Context menu", 0, 0, 0);
  ST7565_BlitFullScreen();
}

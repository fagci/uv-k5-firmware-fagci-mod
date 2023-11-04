#include "ook.h"

static uint16_t counter;

void OOK_update() { counter++; }

void OOK_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {}

void OOK_render() {
  UI_ClearAppScreen();

  UI_PrintStringSmallest("OOK app", 0, 32, false, true);

  ST7565_BlitFullScreen();
}

#include "appmenu.h"

void APPMENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  uint8_t index = Key - KEY_0;
  switch (Key) {
  case KEY_EXIT:
    gRequestDisplayScreen = DISPLAY_MAIN;
    break;
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
    if (index < ARRAY_SIZE(appsNames)) {
      gAppToDisplay = index;
      gRequestDisplayScreen = DISPLAY_MAIN;
    }
    break;
  default:
    break;
  }
}

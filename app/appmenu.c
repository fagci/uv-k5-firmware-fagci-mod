#include "appmenu.h"

void APPMENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  switch (Key) {
  case KEY_EXIT:
    gRequestDisplayScreen = DISPLAY_MAIN;
    break;
  case KEY_1:
    gAppToDisplay = APP_SPLIT;
    gRequestDisplayScreen = DISPLAY_MAIN;
    break;
  case KEY_2:
    gAppToDisplay = APP_OOK;
    gRequestDisplayScreen = DISPLAY_MAIN;
    break;
  default:
    break;
  }
}

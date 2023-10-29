#include "contextmenu.h"

void CONTEXTMENU_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  switch (Key) {
  case KEY_EXIT:
    gRequestDisplayScreen = DISPLAY_MAIN;
    break;
  default:
    break;
  }
}

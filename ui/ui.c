/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "ui.h"
#include "../app/dtmf.h"
#include <string.h>
#if defined(ENABLE_FMRADIO)
#include "app/fm.h"
#endif
#include "../app/scanner.h"
#include "../misc.h"
#if defined(ENABLE_AIRCOPY)
#include "aircopy.h"
#endif
#include "../apps/abscanner.h"
#include "../apps/scanlist.h"
#include "appmenu.h"
#include "contextmenu.h"
#include "fmradio.h"
#include "inputbox.h"
#include "main.h"
#include "menu.h"
#include "scanner.h"
#include "split.h"

GUI_DisplayType_t gScreenToDisplay;
GUI_DisplayType_t gRequestDisplayScreen = DISPLAY_INVALID;
GUI_AppType_t gAppToDisplay = APP_SPLIT;

const App apps[4] = {
    {""},
    {"Split"},
    {"Scanner"},
    {"Scanlist", NULL, SCANLIST_update, SCANLIST_render, SCANLIST_key},
    /* {"A to B scanner", ABSCANNER_init, ABSCANNER_update, ABSCANNER_render,
     ABSCANNER_key}, */
};

uint8_t gAskForConfirmation;
bool gAskToSave;
bool gAskToDelete;

void UI_DisplayApp(void) {
  if (gAppToDisplay) {
    if (apps[gAppToDisplay].render) {
      apps[gAppToDisplay].render();
    }
  }
}

void GUI_DisplayScreen(void) {
  switch (gScreenToDisplay) {
  case DISPLAY_MAIN:
    if (gAppToDisplay != APP_SCANLIST) {
      UI_DisplayMain();
    }
    UI_DisplayApp();
    break;
#if defined(ENABLE_FMRADIO)
  case DISPLAY_FM:
    UI_DisplayFM();
    break;
#endif
  case DISPLAY_MENU:
    UI_DisplayMenu();
    break;
  case DISPLAY_CONTEXT_MENU:
    UI_DisplayContextMenu();
    break;
  case DISPLAY_APP_MENU:
    UI_DisplayAppMenu();
    break;
#if defined(ENABLE_AIRCOPY)
  case DISPLAY_AIRCOPY:
    UI_DisplayAircopy();
    break;
#endif
  default:
    break;
  }
}

void GUI_SelectNextDisplay(GUI_DisplayType_t Display) {
  if (Display != DISPLAY_INVALID) {
    if (gScreenToDisplay != Display) {
      gInputBoxIndex = 0;
      gIsInSubMenu = false;
      gCssScanMode = CSS_SCAN_MODE_OFF;
      gScanState = SCAN_OFF;
#if defined(ENABLE_FMRADIO)
      gFM_ScanState = FM_SCAN_OFF;
#endif
      gAskForConfirmation = 0;
      gDTMF_InputMode = false;
      gDTMF_InputIndex = 0;
      gF_LOCK = false;
      gAskToSave = false;
      gAskToDelete = false;
      if (gWasFKeyPressed) {
        gWasFKeyPressed = false;
        gUpdateStatus = true;
      }
    }
    gUpdateDisplay = true;
    gScreenToDisplay = Display;
  }
}

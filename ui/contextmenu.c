#include "contextmenu.h"

typedef struct MenuItem {
  const char *name;
  void (*getValue)(char *string);
} MenuItem;

void getStepValue(char *String) {
  uint8_t stepIndex = gTxVfo->STEP_SETTING;
  sprintf(String, "%d.%02dK", StepFrequencyTable[stepIndex] / 100,
          StepFrequencyTable[stepIndex] % 100);
}

MenuItem vfoMenu[] = {
    {"Step", getStepValue},
    {"BW"},
    {""},
    {""},
    {"SCR"},
    {"MIC"},
    {"T DCS"},
    {"T CT"},
    {""},
    {""},
    {"R DCS"},
    {"R CT"},
};

const char chars[] = "123*4560789F";

void UI_DisplayContextMenu() {
  char String[16];

  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  if (IS_MR_CHANNEL(gTxVfo->CHANNEL_SAVE)) {
    sprintf(String, "MR channel menu");
  } else {
    sprintf(String, "VFO menu");
  }
  UI_PrintStringSmallest(String, 0, 1, 0, true);

  for (uint8_t i = 0; i < 12; ++i) {
    uint8_t row = (i / 4);
    uint8_t line = row * 2 + 1;
    uint8_t sy = row * 16 + 8;
    uint8_t sx = (i % 4) * 32;

    for (uint8_t j = 0; j < 5; ++j) {
      gFrameBuffer[line][sx + j] = 127;
    }

    sprintf(String, "%c", chars[i]);
    UI_PrintStringSmallest(String, sx + 1, sy + 1, false, false);

    UI_PrintStringSmallest(vfoMenu[i].name, sx + 7, sy + 1, false, true);

    if (vfoMenu[i].getValue) {
      vfoMenu[i].getValue(String);
      UI_PrintStringSmallest(String, sx + 7, sy + 7, false, true);
    }
  }

  ST7565_BlitFullScreen();
}

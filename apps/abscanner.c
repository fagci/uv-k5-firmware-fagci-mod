#include "abscanner.h"

// static uint32_t bounds[2];

void ABSCANNER_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {}

void ABSCANNER_init(void) {
  /* VFO_Info_t *vfo = gEeprom.VfoInfo;
  uint32_t f1 = vfo[0].pTX->Frequency;
  uint32_t f2 = vfo[1].pTX->Frequency; */
}

void ABSCANNER_update(void) {}

void ABSCANNER_render(void) {
  /* char String[16];
  VFO_Info_t *vfo = gEeprom.VfoInfo;

  UI_ClearAppScreen();

  for (uint8_t i = 0; i < 2; ++i) {
    sprintf(String, "%u.%05u", vfo[i].pTX->Frequency / 100000,
            vfo[i].pTX->Frequency % 100000);
    UI_PrintStringSmallest(String, 0, i * 8 + 36, false, true);
  } */
}

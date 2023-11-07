#include "scanlist.h"
#include "../driver/eeprom.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../settings.h"
#include "../ui/ui.h"
#include <stdio.h>

static uint16_t cursor;

void scanListUpdate(uint8_t Channel, uint8_t val) {
  uint8_t State[8];
  uint16_t Offset = 0x0D60 + (Channel & ~7U);

  gMR_ChannelAttributes[Channel] &= ~(MR_CH_SCANLIST1 | MR_CH_SCANLIST2);
  if (val & 1) {
    gMR_ChannelAttributes[Channel] |= MR_CH_SCANLIST1;
  }
  if (val & 2) {
    gMR_ChannelAttributes[Channel] |= MR_CH_SCANLIST2;
  }

  EEPROM_ReadBuffer(Offset, State, sizeof(State));
  State[Channel & 7U] = gMR_ChannelAttributes[Channel];
  EEPROM_WriteBuffer(Offset, State);
}

void SCANLIST_update() {}

void SCANLIST_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  if (bKeyPressed) {
    switch (Key) {
    case KEY_DOWN:
      if (cursor == 199) {
        cursor = 0;
      } else {
        cursor++;
      }
      break;
    case KEY_UP:
      if (cursor == 0) {
        cursor = 199;
      } else {
        cursor--;
      }
      break;
    case KEY_F:
      if (cursor < 189) {
        cursor += 10;
      } else {
        cursor = 0;
      }
      break;
    case KEY_STAR:
      if (cursor > 10) {
        cursor -= 10;
      } else {
        cursor = 199;
      }
      break;
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
      scanListUpdate(cursor, Key - KEY_0);
      break;
    case KEY_EXIT:
      gAppToDisplay = APP_SPLIT;
      gRequestDisplayScreen = DISPLAY_MAIN;
      break;
    default:
      break;
    }
    gUpdateDisplay = true;
  }
}

void SCANLIST_render() {
  char String[32];
  char channelName[16];
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  const uint8_t count = 200;
  const uint8_t perScreen = 7;
  const uint8_t offset = Clamp(cursor - 2, 0, count - perScreen);
  for (uint8_t i = 0; i < perScreen; ++i) {
    uint8_t itemIndex = i + offset;
    uint8_t chNum = itemIndex + 1;
    uint8_t y = i * 8 + 1;
    uint8_t *pLine = gFrameBuffer[i];

    bool isCurrent = cursor == itemIndex;

    if (isCurrent) {
      memset(pLine, 127, LCD_WIDTH);
    }

    GetChannelName(itemIndex, channelName);

    if (UI_NoChannelName(channelName)) {
      sprintf(channelName, "CH-%03u", chNum);
    }
    sprintf(String, "%03u: %s", chNum, channelName);

    UI_PrintStringSmallest(String, 1, y, false, !isCurrent);

    UI_DrawScanListFlag(pLine, gMR_ChannelAttributes[itemIndex]);
  }

  ST7565_BlitFullScreen();
}

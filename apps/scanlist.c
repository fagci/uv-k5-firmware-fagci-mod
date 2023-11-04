#include "scanlist.h"
#include "../driver/eeprom.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../settings.h"
#include "../ui/ui.h"
#include <stdio.h>

static uint16_t cursor;

void SCANLIST_update() {}

void SCANLIST_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  if (bKeyPressed) {
    switch (Key) {
    case KEY_DOWN:
      if (cursor < 200) {
        cursor++;
      } else {
        cursor = 0;
      }
      break;
    case KEY_UP:
      if (cursor > 0) {
        cursor--;
      } else {
        cursor = 200;
      }
      break;
    case KEY_1:
      gMR_ChannelAttributes[cursor] &= ~(MR_CH_SCANLIST1 | MR_CH_SCANLIST2);
      gMR_ChannelAttributes[cursor] |= MR_CH_SCANLIST1;
      break;
    case KEY_2:
      gMR_ChannelAttributes[cursor] &= ~(MR_CH_SCANLIST1 | MR_CH_SCANLIST2);
      gMR_ChannelAttributes[cursor] |= MR_CH_SCANLIST2;
      break;
    case KEY_3:
      gMR_ChannelAttributes[cursor] |= MR_CH_SCANLIST1 | MR_CH_SCANLIST2;
      break;
    case KEY_0:
      gMR_ChannelAttributes[cursor] &= ~(MR_CH_SCANLIST1 | MR_CH_SCANLIST2);
      break;
    case KEY_MENU:
      EEPROM_WriteBuffer(0x0D60, gMR_ChannelAttributes);
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
  UI_ClearAppScreen();

  const uint8_t count = 200;
  const uint8_t perScreen = 3;
  const uint8_t offset = Clamp(cursor - 2, 0, count - perScreen);
  for (uint8_t i = 0; i < perScreen; ++i) {
    uint8_t itemIndex = i + offset;
    uint8_t attrs = gMR_ChannelAttributes[itemIndex];
    uint8_t chPos = 4 + i;
    bool isCurrent = cursor == i + offset;
    uint8_t chNum = itemIndex + 1;
    uint8_t y = 33 + i * 8;

    bool s1 = attrs & MR_CH_SCANLIST1;
    bool s2 = attrs & MR_CH_SCANLIST2;

    if (isCurrent) {
      memset(gFrameBuffer[chPos], 0xFF, LCD_WIDTH);
    }

    memset(channelName, 0, sizeof(channelName));
    EEPROM_ReadBuffer(0x0F50 + (itemIndex * 0x10), channelName, 10);
    bool noChannelName = channelName[0] < 32 || channelName[0] > 127;

    if (noChannelName) {
      sprintf(channelName, "CH-%03u", chNum);
    }
    sprintf(String, "%03u: %s", chNum, channelName);
    UI_PrintStringSmallest(String, 0, y, false, !isCurrent);

    if (s1) {
      gFrameBuffer[chPos][117] ^= 0b100010;
      gFrameBuffer[chPos][118] ^= 0b111110;
      gFrameBuffer[chPos][119] ^= 0b100010;
    }
    if (s2) {
      gFrameBuffer[chPos][122] ^= 0b100010;
      gFrameBuffer[chPos][123] ^= 0b111110;
      gFrameBuffer[chPos][124] ^= 0b100010;
      gFrameBuffer[chPos][125] ^= 0b111110;
      gFrameBuffer[chPos][126] ^= 0b100010;
    }
  }

  ST7565_BlitFullScreen();
}

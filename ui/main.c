/* Copyright 2023 Dual Tachyon, fagci
 * https://github.com/DualTachyon
 * https://github.com/fagci
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

#include "../ui/main.h"
#include "../app/dtmf.h"
#include "../app/finput.h"
#include "../bitmaps.h"
#include "../driver/bk4819.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../frequencies.h"
#include "../functions.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/helper.h"
#include "../ui/inputbox.h"
#include "../ui/rssi.h"
#include "../ui/ui.h"
#include <string.h>

static void displayVfoStatus(uint8_t vfoNum, bool isTx) {
  VFO_Info_t vfo = gEeprom.VfoInfo[vfoNum];
  uint8_t lineSubY = vfoNum == 0 ? 16 : 48;
  if (vfo.ModulationType == MOD_FM) {
    UI_PrintStringSmallest(dcsNames[(isTx ? vfo.pTX : vfo.pRX)->CodeType], 21,
                           lineSubY, false, true);
  }

  UI_PrintStringSmallest(powerNames[vfo.OUTPUT_POWER], 35, lineSubY, false,
                         true);

  UI_PrintStringSmallest(deviationNames[vfo.OFFSET_DIR], 54, lineSubY, false,
                         true);

  if (vfo.FrequencyReverse) {
    UI_PrintStringSmallest("R", 64, lineSubY, false, true);
  }
  UI_PrintStringSmallest(bwNames[vfo.CHANNEL_BANDWIDTH], 60, lineSubY, false,
                         true);
  if (vfo.DTMF_DECODING_ENABLE) {
    UI_PrintStringSmallest("DTMF", 81, lineSubY, false, true);
  }
  if (vfo.SCRAMBLING_TYPE && gSetting_ScrambleEnable) {
    UI_PrintStringSmallest("SCR", 98, lineSubY, false, true);
  }
}

static void displayDtmf(uint8_t vfoNum) {
  char String[16];
  char Contact[16];

  if (!gDTMF_InputMode) {
    if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT) {
      if (gDTMF_State == DTMF_STATE_CALL_OUT_RSP) {
        strcpy(String, "CALL OUT(RSP)");
      } else {
        strcpy(String, "CALL OUT");
      }
    } else if (gDTMF_CallState == DTMF_CALL_STATE_RECEIVED) {
      if (DTMF_FindContact(gDTMF_Caller, Contact)) {
        sprintf(String, "CALL:%s", Contact);
      } else {
        sprintf(String, "CALL:%s", gDTMF_Caller);
      }
    } else if (gDTMF_IsTx) {
      if (gDTMF_State == DTMF_STATE_TX_SUCC) {
        strcpy(String, "DTMF TX(SUCC)");
      } else {
        strcpy(String, "DTMF TX");
      }
    }
  } else {
    sprintf(String, ">%s", gDTMF_InputBox);
  }
  UI_PrintString(String, 2, 127, vfoNum * 3, 8, false);

  memset(String, 0, sizeof(String));
  memset(Contact, 0, sizeof(Contact));

  if (!gDTMF_InputMode) {
    if (gDTMF_CallState == DTMF_CALL_STATE_CALL_OUT) {
      if (DTMF_FindContact(gDTMF_String, Contact)) {
        sprintf(String, ">%s", Contact);
      } else {
        sprintf(String, ">%s", gDTMF_String);
      }
    } else if (gDTMF_CallState == DTMF_CALL_STATE_RECEIVED) {
      if (DTMF_FindContact(gDTMF_Callee, Contact)) {
        sprintf(String, ">%s", Contact);
      } else {
        sprintf(String, ">%s", gDTMF_Callee);
      }
    } else if (gDTMF_IsTx) {
      sprintf(String, ">%s", gDTMF_String);
    }
  }
  UI_PrintString(String, 2, 127, 2 + (vfoNum * 3), 8, false);
}

static void displayVfo(uint8_t vfoNum) {
  char String[16];
  bool filled = false;
  uint8_t Line = vfoNum * 4;

  uint8_t Channel = gEeprom.TX_VFO;
  bool bIsSameVfo = Channel == vfoNum;
  VFO_Info_t vfoInfo = gEeprom.VfoInfo[vfoNum];
  uint8_t screenCH = gEeprom.ScreenChannel[vfoNum];
  uint8_t *pLine0 = gFrameBuffer[Line];

  if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF && gRxVfoIsActive) {
    Channel = gEeprom.RX_VFO;
  }

  if (Channel != vfoNum) {
    if (gDTMF_CallState != DTMF_CALL_STATE_NONE || gDTMF_IsTx ||
        gDTMF_InputMode) {
      displayDtmf(vfoNum);
      return;
    }

    if (bIsSameVfo) {
      // Default
      filled = true;
      memset(pLine0, 127, 19);
    }
  } else {
    if (bIsSameVfo) {
      // Default
      filled = true;
      memset(pLine0, 127, 19);
    } else {
      // Not default
      pLine0[0] = 0b01111111;
      pLine0[1] = 0b01000001;
      pLine0[17] = 0b01000001;
      pLine0[18] = 0b01111111;
    }
  }

  if (gCurrentFunction == FUNCTION_TRANSMIT) {
    Channel = gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF ? gEeprom.RX_VFO
                                                         : gEeprom.TX_VFO;
    if (Channel == vfoNum) {
      UI_PrintStringSmallBold("TX", 0, 0, Line + 1);
    }
  } else if ((gCurrentFunction == FUNCTION_RECEIVE ||
              gCurrentFunction == FUNCTION_MONITOR) &&
             gEeprom.RX_VFO == vfoNum) {
    UI_PrintStringSmallBold("RX", 0, 0, Line + 1);
  }

  if (IS_MR_CHANNEL(screenCH)) {
    if (gInputBoxIndex == 0 || gEeprom.TX_VFO != vfoNum) {
      sprintf(String, "M%03d", screenCH + 1);
    } else {
      sprintf(String, "M---");
      // TODO: temporary
      for (uint8_t j = 0; j < 3; j++) {
        char v = gInputBox[j];
        String[j + 1] = v == 10 ? '-' : v + '0';
      }
    }
  } else {
    sprintf(String, "VFO");
  }
  UI_PrintStringSmallest(String, 2, Line * 8 + 1, false, !filled);

  uint8_t State = VfoState[vfoNum];
  if (State) {
    strcpy(String, vfoStateNames[State]);
    UI_PrintString(String, 31, 111, vfoNum * 4, 8, true);
    return;
  }
  if (freqInputIndex && IS_FREQ_CHANNEL(screenCH) && gEeprom.TX_VFO == vfoNum) {
    UI_PrintString(freqInputString, 24, 127, vfoNum * 4, 8, true);
  } else {
    uint32_t frequency = vfoInfo.pRX->Frequency;

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
      if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) {
        Channel = gEeprom.RX_VFO;
      } else {
        Channel = gEeprom.TX_VFO;
      }
      if (Channel == vfoNum) {
        frequency = vfoInfo.pTX->Frequency;
      }
    }

    if (IS_MR_CHANNEL(screenCH)) {
      UI_DrawScanListFlag(gFrameBuffer[Line + 2],
                          gMR_ChannelAttributes[screenCH]);
    }

    bool noChannelName = UI_NoChannelName(vfoInfo.Name);
    sprintf(String, "CH-%03u", screenCH + 1);

    frequency = GetScreenF(frequency);

    if (!IS_MR_CHANNEL(screenCH) ||
        gEeprom.CHANNEL_DISPLAY_MODE == MDF_FREQUENCY) {
      NUMBER_ToDigits(frequency, String);
      UI_DisplayFrequency(String, 19, Line, false, false);
      UI_DisplaySmallDigits(2, String + 7, 113, Line + 1);
    } else if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_CHANNEL ||
               (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME && noChannelName)) {
      UI_PrintString(String, 31, 112, vfoNum * 4, 8, true);
    } else if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME_FREQ) {
      // no channel name, show channel number instead
      if (!noChannelName) {
        memset(String, 0, sizeof(String));
        memmove(String, vfoInfo.Name, 10);
      }
      UI_PrintStringSmallBold(String, 31 + 8, 0, Line);

      // show the channel frequency below the channel number/name
      sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
      UI_PrintStringSmall(String, 31 + 8, 0, Line + 1);
    } else {
      UI_PrintString(vfoInfo.Name, 31, 112, vfoNum * 4, 8, true);
    }
  }

  UI_PrintStringSmallest(modulationTypeOptions[vfoInfo.ModulationType], 116,
                         2 + vfoNum * 32, false, true);
}

void UI_DisplayMain(void) {
  uint8_t i;

  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  if (gEeprom.KEY_LOCK && gKeypadLocked) {
    UI_PrintString("Long Press #", 0, 127, 1, 8, true);
    UI_PrintString("To Unlock", 0, 127, 3, 8, true);
    ST7565_BlitFullScreen();
    return;
  }

  for (i = 0; i < 2; i++) {
    uint32_t isTx = false;

    uint8_t Channel;
    if (gCurrentFunction == FUNCTION_TRANSMIT) {
      if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) {
        Channel = gEeprom.RX_VFO;
      } else {
        Channel = gEeprom.TX_VFO;
      }
      if (Channel == i) {
        isTx = true;
      }
    } else {
      if ((gCurrentFunction == FUNCTION_RECEIVE ||
           gCurrentFunction == FUNCTION_MONITOR) &&
          gEeprom.RX_VFO == i) {
      }
    }
    displayVfo(i);
    displayVfoStatus(i, isTx);
  }

  if (gScreenToDisplay == DISPLAY_MAIN && !gKeypadLocked) {
    if (gCurrentFunction == FUNCTION_RECEIVE ||
        gCurrentFunction == FUNCTION_MONITOR ||
        gCurrentFunction == FUNCTION_INCOMING) {
      UI_DisplayRSSIBar(BK4819_GetRSSI());
    }
  }

  ST7565_BlitFullScreen();
}

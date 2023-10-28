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

#include "../ui/main.h"
#include "../app/dtmf.h"
#include "../app/finput.h"
#include "../bitmaps.h"
#include "../driver/bk4819.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
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

void UI_DisplayMain(void) {
  char String[16];
  uint8_t i;

  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
  if (gEeprom.KEY_LOCK && gKeypadLocked) {
    UI_PrintString("Long Press #", 0, 127, 1, 8, true);
    UI_PrintString("To Unlock", 0, 127, 3, 8, true);
    ST7565_BlitFullScreen();
    return;
  }

  for (i = 0; i < 2; i++) {
    bool filled = false;
    uint8_t Line = i * 4;
    uint8_t lineSubY = (Line + 2) * 8;

    uint8_t Channel = gEeprom.TX_CHANNEL;
    bool bIsSameVfo = !!(Channel == i);
    VFO_Info_t vfoInfo = gEeprom.VfoInfo[i];
    uint8_t screenCH = gEeprom.ScreenChannel[i];
    uint8_t *pLine0 = gFrameBuffer[Line];

    if (gEeprom.DUAL_WATCH != DUAL_WATCH_OFF && gRxVfoIsActive) {
      Channel = gEeprom.RX_CHANNEL;
    }

    if (Channel != i) {
      if (gDTMF_CallState != DTMF_CALL_STATE_NONE || gDTMF_IsTx ||
          gDTMF_InputMode) {
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
        UI_PrintString(String, 2, 127, i * 3, 8, false);

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
        UI_PrintString(String, 2, 127, 2 + (i * 3), 8, false);
        continue;
      } else if (bIsSameVfo) {
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

    // 0x8EE2
    uint32_t SomeValue = 0;

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
      if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) {
        Channel = gEeprom.RX_CHANNEL;
      } else {
        Channel = gEeprom.TX_CHANNEL;
      }
      if (Channel == i) {
        SomeValue = 1;
        UI_PrintStringSmallBold("TX", 0, 0, Line + 1);
      }
    } else {
      SomeValue = 2;
      if ((gCurrentFunction == FUNCTION_RECEIVE ||
           gCurrentFunction == FUNCTION_MONITOR) &&
          gEeprom.RX_CHANNEL == i) {
        UI_PrintStringSmallBold("RX", 0, 0, Line + 1);
      }
    }

    if (IS_MR_CHANNEL(screenCH)) {
      if (gInputBoxIndex == 0 || gEeprom.TX_CHANNEL != i) {
        sprintf(String, "M%03d", screenCH + 1);
      } else {
        sprintf(String, "M---");
        // TODO: temporary
        for (uint8_t j = 0; j < 3; j++) {
          char v = gInputBox[j];
          String[j + 1] = v == 10 ? '-' : v + '0';
        }
      }
      UI_PrintStringSmallest(String, 2, Line * 8 + 1, false, !filled);
    } else {
      UI_PrintStringSmallest("VFO", 4, Line * 8 + 1, false, !filled);
    }

    uint8_t State = VfoState[i];
    if (State) {
      uint8_t Width = 10;

      strcpy(String, vfoStateNames[State]);

      if (State == VFO_STATE_BUSY) {
        Width = 15;
      } else if (State == VFO_STATE_VOL_HIGH) {
        Width = 8;
      }

      UI_PrintString(String, 31, 111, i * 4, Width, true);
    } else {
      if (freqInputIndex && IS_FREQ_CHANNEL(screenCH) &&
          gEeprom.TX_CHANNEL == i) {
        UI_PrintString(freqInputString, 24, 127, i * 4, 8, true);
      } else {
        uint32_t frequency = vfoInfo.pRX->Frequency;
        bool noChannelName = vfoInfo.Name[0] == 0 || vfoInfo.Name[0] == 255;

        if (gCurrentFunction == FUNCTION_TRANSMIT) {
          if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) {
            Channel = gEeprom.RX_CHANNEL;
          } else {
            Channel = gEeprom.TX_CHANNEL;
          }
          if (Channel == i) {
            frequency = vfoInfo.pTX->Frequency;
          }
        }

        if (IS_MR_CHANNEL(screenCH)) {
          const uint8_t ATTR = gMR_ChannelAttributes[screenCH];
          if (ATTR & MR_CH_SCANLIST1) {
            UI_PrintStringSmallest("s1", 112, lineSubY, false, true);
          }
          if (ATTR & MR_CH_SCANLIST2) {
            UI_PrintStringSmallest("s2", 120, lineSubY, false, true);
          }
        }

        sprintf(String, "CH-%03u", screenCH + 1);

        if (!IS_MR_CHANNEL(screenCH) ||
            gEeprom.CHANNEL_DISPLAY_MODE == MDF_FREQUENCY) {
          NUMBER_ToDigits(frequency, String);
          UI_DisplayFrequency(String, 19, Line, false, false);
          UI_DisplaySmallDigits(2, String + 7, 113, Line + 1);
        } else if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_CHANNEL ||
                   (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME &&
                    noChannelName)) {
          UI_PrintString(String, 31, 112, i * 4, 8, true);
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
          UI_PrintString(vfoInfo.Name, 31, 112, i * 4, 8, true);
        }
      }
    }

    if (vfoInfo.ModulationType == MOD_FM) {
      const FREQ_Config_t *pConfig = SomeValue == 1 ? vfoInfo.pTX : vfoInfo.pRX;

      UI_PrintStringSmallest(dcsNames[pConfig->CodeType], 21, lineSubY, false,
                             true);
    }
    UI_PrintStringSmallest(modulationTypeOptions[vfoInfo.ModulationType], 116,
                           2 + i * 32, false, true);

    UI_PrintStringSmallest(powerNames[vfoInfo.OUTPUT_POWER], 35, lineSubY,
                           false, true);

    UI_PrintStringSmallest(deviationNames[vfoInfo.FREQUENCY_DEVIATION_SETTING],
                           54, lineSubY, false, true);

    if (vfoInfo.FrequencyReverse) {
      UI_PrintStringSmallest("R", 64, lineSubY, false, true);
    }
    UI_PrintStringSmallest(bwNames[vfoInfo.CHANNEL_BANDWIDTH], 60, lineSubY,
                           false, true);
    if (vfoInfo.DTMF_DECODING_ENABLE) {
      UI_PrintStringSmallest("DTMF", 81, lineSubY, false, true);
    }
    if (vfoInfo.SCRAMBLING_TYPE && gSetting_ScrambleEnable) {
      UI_PrintStringSmallest("SCR", 98, lineSubY, false, true);
    }
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

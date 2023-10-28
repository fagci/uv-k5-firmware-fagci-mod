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
    uint8_t *pLine0;
    // uint8_t *pLine1;
    uint8_t Line;
    uint8_t Channel;
    bool bIsSameVfo;
    VFO_Info_t vfoInfo = gEeprom.VfoInfo[i];
    uint8_t screenCH = gEeprom.ScreenChannel[i];

    if (i == 0) {
      pLine0 = gFrameBuffer[0];
      // pLine1 = gFrameBuffer[1];
      Line = 0;
    } else {
      pLine0 = gFrameBuffer[4];
      // pLine1 = gFrameBuffer[5];
      Line = 4;
    }

    Channel = gEeprom.TX_CHANNEL;
    bIsSameVfo = !!(Channel == i);

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
        // memcpy(pLine0 + 2, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
      }
    } else {
      /* if (bIsSameVfo) {
        memcpy(pLine0 + 2, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
      } else {
        memcpy(pLine0 + 2, BITMAP_VFO_NotDefault,
               sizeof(BITMAP_VFO_NotDefault));
      } */
    }

    // 0x8EE2
    uint32_t SomeValue = 0;

    if (gCurrentFunction == FUNCTION_TRANSMIT) {
#if defined(ENABLE_ALARM)
      if (gAlarmState == ALARM_STATE_ALARM) {
        SomeValue = 2;
      } else {
#else
      if (1) {
#endif
        if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) {
          Channel = gEeprom.RX_CHANNEL;
        } else {
          Channel = gEeprom.TX_CHANNEL;
        }
        if (Channel == i) {
          SomeValue = 1;
          UI_PrintStringSmallest("TX", 20, Line * 8 + 1, false, true);
        }
      }
    } else {
      SomeValue = 2;
      if ((gCurrentFunction == FUNCTION_RECEIVE ||
           gCurrentFunction == FUNCTION_MONITOR) &&
          gEeprom.RX_CHANNEL == i) {
        UI_PrintStringSmallest("RX", 20, Line * 8 + 1, false, true);
      }
    }

    // 0x8F3C
    bool isActiveChannel = Channel == i;
    if (isActiveChannel) {
      memset(gFrameBuffer[Line], 127, 19);
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
      UI_PrintStringSmallest(String, 2, Line * 8 + 1, false, !isActiveChannel);
    } else {
      UI_PrintStringSmallest("VFO", 4, Line * 8 + 1, false, !isActiveChannel);
    }

    // 0x8FEC

    uint8_t State = VfoState[i];
#if defined(ENABLE_ALARM)
    if (gCurrentFunction == FUNCTION_TRANSMIT &&
        gAlarmState == ALARM_STATE_ALARM) {
      if (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF) {
        Channel = gEeprom.RX_CHANNEL;
      } else {
        Channel = gEeprom.TX_CHANNEL;
      }
      if (Channel == i) {
        State = VFO_STATE_ALARM;
      }
    }
#endif
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

        sprintf(String, "CH-%03u", screenCH + 1);

        if (!IS_MR_CHANNEL(screenCH) ||
            gEeprom.CHANNEL_DISPLAY_MODE == MDF_FREQUENCY) {
          NUMBER_ToDigits(frequency, String);
          UI_DisplayFrequency(String, 18, Line, false, false);
          UI_DisplaySmallDigits(2, String + 7, 113, Line + 1);

          if (IS_MR_CHANNEL(screenCH)) {
            const uint8_t Attr = gMR_ChannelAttributes[screenCH];
            if (Attr & MR_CH_SCANLIST1) {
              pLine0[114] = 0b11000000;
            }
            if (Attr & MR_CH_SCANLIST2) {
              pLine0[114] |= 0b00011000;
            }
          }
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
          UI_PrintStringSmall(String, 31 + 8, 0, Line);

          // show the channel frequency below the channel number/name
          sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
          UI_PrintStringSmall(String, 31 + 8, 0, Line + 1);
        } else {
          UI_PrintString(vfoInfo.Name, 31, 112, i * 4, 8, true);
        }
      }
    }

    uint8_t lineSubY = (Line + 2) * 8;

    // 0x931E
    UI_PrintStringSmallest(modulationTypeOptions[vfoInfo.ModulationType], 116,
                           2 + i * 32, false, true);

    if (vfoInfo.ModulationType == MOD_FM) {
      const FREQ_Config_t *pConfig = SomeValue == 1 ? vfoInfo.pTX : vfoInfo.pRX;

      UI_PrintStringSmallest(dcsNames[pConfig->CodeType], 27, lineSubY, false,
                             true);
    }

    // 0x936C
    UI_PrintStringSmallest(powerNames[vfoInfo.OUTPUT_POWER], 40, lineSubY,
                           false, true);

    if (vfoInfo.ConfigRX.Frequency != vfoInfo.ConfigTX.Frequency) {
      UI_PrintStringSmallest(
          deviationNames[vfoInfo.FREQUENCY_DEVIATION_SETTING], 60, lineSubY,
          false, true);
    }

    if (vfoInfo.FrequencyReverse) {
      UI_PrintStringSmallest("R", 64, lineSubY, false, true);
    }
    UI_PrintStringSmallest(bwNames[vfoInfo.CHANNEL_BANDWIDTH], 64, lineSubY,
                           false, true);
    if (vfoInfo.DTMF_DECODING_ENABLE) {
      UI_PrintStringSmallest("DTMF", 84, lineSubY, false, true);
    }
    if (vfoInfo.SCRAMBLING_TYPE && gSetting_ScrambleEnable) {
      UI_PrintStringSmallest("SCR", 110, lineSubY, false, true);
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

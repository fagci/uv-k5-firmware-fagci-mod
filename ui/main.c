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
#include "../ui/ui.h"
#include <string.h>

#if defined(ENABLE_RSSIBAR)
void UI_DisplayRSSIBar(int16_t rssi) {
  char String[16];

  const uint8_t LINE = 3;
  const uint8_t BAR_LEFT_MARGIN = 24;

  int dBm = Rssi2DBm(rssi);
  uint8_t s = DBm2S(dBm);
  uint8_t *line = gFrameBuffer[LINE];

  memset(line, 0, 128);

  for (int i = BAR_LEFT_MARGIN, sv = 1; i < BAR_LEFT_MARGIN + s * 4;
       i += 4, sv++) {
    line[i] = line[i + 2] = 0b00111110;
    line[i + 1] = sv > 9 ? 0b00100010 : 0b00111110;
  }

  sprintf(String, "%d", dBm);
  UI_PrintStringSmallest(String, 110, 25, false, true);
  if (s < 10) {
    sprintf(String, "S%u", s);
  } else {
    sprintf(String, "S9+%u0", s - 9);
  }
  UI_PrintStringSmallest(String, 3, 25, false, true);
  ST7565_BlitFullScreen();
}
#endif

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
    uint8_t *pLine1;
    uint8_t Line;
    uint8_t Channel;
    bool bIsSameVfo;
    VFO_Info_t vfoInfo = gEeprom.VfoInfo[i];

    if (i == 0) {
      pLine0 = gFrameBuffer[0];
      pLine1 = gFrameBuffer[1];
      Line = 0;
    } else {
      pLine0 = gFrameBuffer[4];
      pLine1 = gFrameBuffer[5];
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
        memcpy(pLine0 + 2, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
      }
    } else {
      if (bIsSameVfo) {
        memcpy(pLine0 + 2, BITMAP_VFO_Default, sizeof(BITMAP_VFO_Default));
      } else {
        memcpy(pLine0 + 2, BITMAP_VFO_NotDefault,
               sizeof(BITMAP_VFO_NotDefault));
      }
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
          memcpy(pLine0 + 14, BITMAP_TX, sizeof(BITMAP_TX));
        }
      }
    } else {
      SomeValue = 2;
      if ((gCurrentFunction == FUNCTION_RECEIVE ||
           gCurrentFunction == FUNCTION_MONITOR) &&
          gEeprom.RX_CHANNEL == i) {
        memcpy(pLine0 + 14, BITMAP_RX, sizeof(BITMAP_RX));
      }
    }

    // 0x8F3C
    if (IS_MR_CHANNEL(gEeprom.ScreenChannel[i])) {
      memcpy(pLine1 + 2, BITMAP_M, sizeof(BITMAP_M));
      if (gInputBoxIndex == 0 || gEeprom.TX_CHANNEL != i) {
        NUMBER_ToDigits(gEeprom.ScreenChannel[i] + 1, String);
      } else {
        memcpy(String + 5, gInputBox, 3);
      }
      UI_DisplaySmallDigits(3, String + 5, 10, Line + 1);
    } else if (IS_FREQ_CHANNEL(gEeprom.ScreenChannel[i])) {
      char c;

      memcpy(pLine1 + 14, BITMAP_F, sizeof(BITMAP_F));
      c = (gEeprom.ScreenChannel[i] - FREQ_CHANNEL_FIRST) + 1;
      UI_DisplaySmallDigits(1, &c, 22, Line + 1);
    } else {
#if defined(ENABLE_NOAA)
      memcpy(pLine1 + 7, BITMAP_NarrowBand, sizeof(BITMAP_NarrowBand));
      if (gInputBoxIndex == 0 || gEeprom.TX_CHANNEL != i) {
        NUMBER_ToDigits((gEeprom.ScreenChannel[i] - NOAA_CHANNEL_FIRST) + 1,
                        String);
      } else {
        String[6] = gInputBox[0];
        String[7] = gInputBox[1];
      }
      UI_DisplaySmallDigits(2, String + 6, 15, Line + 1);
#endif
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

      memset(String, 0, sizeof(String));
      switch (State) {
      case 1:
        strcpy(String, "BUSY");
        Width = 15;
        break;
      case 2:
        strcpy(String, "BAT LOW");
        break;
      case 3:
        strcpy(String, "DISABLE");
        break;
      case 4:
        strcpy(String, "TIMEOUT");
        break;
#if defined(ENABLE_ALARM)
      case 5:
        strcpy(String, "ALARM");
        break;
#endif
      case 6:
        sprintf(String, "VOL HIGH");
        Width = 8;
        break;
      }
      UI_PrintString(String, 31, 111, i * 4, Width, true);
    } else {
      if (freqInputIndex && IS_FREQ_CHANNEL(gEeprom.ScreenChannel[i]) &&
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

        if (!IS_MR_CHANNEL(gEeprom.ScreenChannel[i]) ||
            gEeprom.CHANNEL_DISPLAY_MODE == MDF_FREQUENCY) {
          sprintf(String, "%4u.%03u", frequency / 100000,
                  frequency / 100 % 1000);
          UI_PrintString(String, 46, 112, i * 4, 8, false);

          /* sprintf(String, "%02u", frequency % 100);
          UI_PrintStringSmallest(String, 116, 8 + i * 32, false, true); */

          NUMBER_ToDigits(frequency, String);
          // UI_DisplayFrequency(String, 32, Line, false, false);
          UI_DisplaySmallDigits(2, String + 6, 113, Line + 1);

          if (IS_MR_CHANNEL(gEeprom.ScreenChannel[i])) {
            const uint8_t Attributes =
                gMR_ChannelAttributes[gEeprom.ScreenChannel[i]];
            if (Attributes & MR_CH_SCANLIST1) {
              pLine0[113] = 0b11000000;
              pLine0[114] = 0b11000000;
            }
            if (Attributes & MR_CH_SCANLIST2) {
              pLine0[113] |= 0b00001100;
              pLine0[114] |= 0b00001100;
            }
          }
        } else if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_CHANNEL) {
          sprintf(String, "CH-%03d", gEeprom.ScreenChannel[i] + 1);
          UI_PrintString(String, 31, 112, i * 4, 8, true);
        } else if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME) {
          if (noChannelName) {
            sprintf(String, "CH-%03d", gEeprom.ScreenChannel[i] + 1);
            UI_PrintString(String, 31, 112, i * 4, 8, true);
          } else {
            UI_PrintString(vfoInfo.Name, 31, 112, i * 4, 8, true);
          }
        } else if (gEeprom.CHANNEL_DISPLAY_MODE == MDF_NAME_FREQ) {
          // no channel name, show channel number instead
          if (noChannelName) {
            sprintf(String, "CH-%03u", gEeprom.ScreenChannel[i] + 1);
          } else { // channel name
            memset(String, 0, sizeof(String));
            memmove(String, vfoInfo.Name, 10);
          }
          UI_PrintStringSmall(String, 31 + 8, 0, Line);

          // show the channel frequency below the channel number/name
          sprintf(String, "%u.%05u", frequency / 100000, frequency % 100000);
          UI_PrintStringSmall(String, 31 + 8, 0, Line + 1);
        }
      }
    }

    // 0x931E
    const char *modulationTypeOptions[] = {" FM", " AM", "SSB"};
    UI_PrintStringSmallest(modulationTypeOptions[vfoInfo.ModulationType], 116,
                           2 + i * 32, false, true);

    if (vfoInfo.ModulationType == MOD_FM) {
      const FREQ_Config_t *pConfig = SomeValue == 1 ? vfoInfo.pTX : vfoInfo.pRX;

      switch (pConfig->CodeType) {
      case CODE_TYPE_CONTINUOUS_TONE:
        UI_PrintStringSmallest("CT", 27, (Line + 2) * 8, false, true);
        break;
      case CODE_TYPE_DIGITAL:
      case CODE_TYPE_REVERSE_DIGITAL:
        UI_PrintStringSmallest("DCS", 24, (Line + 2) * 8, false, true);
        break;
      default:
        break;
      }
    }

    // 0x936C
    char *power[3] = {"LOW", "MID", "HIGH"};
    UI_PrintStringSmallest(power[vfoInfo.OUTPUT_POWER], 44, (Line + 2) * 8,
                           false, true);

    if (vfoInfo.ConfigRX.Frequency != vfoInfo.ConfigTX.Frequency) {
      if (vfoInfo.FREQUENCY_DEVIATION_SETTING == FREQUENCY_DEVIATION_ADD) {
        UI_PrintStringSmallest("+", 60, (Line + 2) * 8, false, true);
      }
      if (vfoInfo.FREQUENCY_DEVIATION_SETTING == FREQUENCY_DEVIATION_SUB) {
        UI_PrintStringSmallest("-", 60, (Line + 2) * 8, false, true);
      }
    }

    if (vfoInfo.FrequencyReverse) {
      UI_PrintStringSmallest("R", 64, (Line + 2) * 8, false, true);
    }
    const char *bwOptions[] = {"  25k", "12.5k", "6.25k"};
    UI_PrintStringSmallest(bwOptions[vfoInfo.CHANNEL_BANDWIDTH], 64,
                           (Line + 2) * 8, false, true);
    if (vfoInfo.DTMF_DECODING_ENABLE) {
      UI_PrintStringSmallest("DTMF", 84, (Line + 2) * 8, false, true);
    }
    if (vfoInfo.SCRAMBLING_TYPE && gSetting_ScrambleEnable) {
      UI_PrintStringSmallest("SCR", 110, (Line + 2) * 8, false, true);
    }
  }

#if defined(ENABLE_RSSIBAR)
  if (gScreenToDisplay == DISPLAY_MAIN && !gKeypadLocked) {
    if (gCurrentFunction == FUNCTION_RECEIVE ||
        gCurrentFunction == FUNCTION_MONITOR ||
        gCurrentFunction == FUNCTION_INCOMING) {
      UI_DisplayRSSIBar(BK4819_GetRSSI());
    }
  }
#endif

  ST7565_BlitFullScreen();
}

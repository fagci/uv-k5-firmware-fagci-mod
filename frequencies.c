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

#include "frequencies.h"
#include "misc.h"
#include "settings.h"

const struct FrequencyBandInfo FrequencyBandTable[7] = {
    [BAND1_50MHz] = {.lower = 1500000, .upper = 10799990},
    [BAND2_108MHz] = {.lower = 10800000, .upper = 13599990},
    [BAND3_136MHz] = {.lower = 13600000, .upper = 17399990},
    [BAND4_174MHz] = {.lower = 17400000, .upper = 34999990},
    [BAND5_350MHz] = {.lower = 35000000, .upper = 39999990},
    [BAND6_400MHz] = {.lower = 40000000, .upper = 46999990},
    [BAND7_470MHz] = {.lower = 47000000, .upper = 134000000},
};

const uint16_t StepFrequencyTable[12] = {
    1,   10,  50,  100,

    250, 500, 625, 833, 1000, 1250, 2500, 10000,
};

FREQUENCY_Band_t FREQUENCY_GetBand(uint32_t Frequency) {
  for (int i = 0; i < ARRAY_SIZE(FrequencyBandTable); i++) {
    if (Frequency >= FrequencyBandTable[i].lower &&
        Frequency <= FrequencyBandTable[i].upper) {
      return i;
    }
  }

  return BAND6_400MHz;
}

uint8_t FREQUENCY_CalculateOutputPower(uint8_t TxpLow, uint8_t TxpMid,
                                       uint8_t TxpHigh, int32_t LowerLimit,
                                       int32_t Middle, int32_t UpperLimit,
                                       int32_t Frequency) {
  if (Frequency <= LowerLimit) {
    return TxpLow;
  }
  if (UpperLimit <= Frequency) {
    return TxpHigh;
  }
  if (Frequency <= Middle) {
    TxpMid +=
        ((TxpMid - TxpLow) * (Frequency - LowerLimit)) / (Middle - LowerLimit);
    return TxpMid;
  }

  TxpMid += ((TxpHigh - TxpMid) * (Frequency - Middle)) / (UpperLimit - Middle);
  return TxpMid;
}

uint32_t FREQUENCY_FloorToStep(uint32_t Upper, uint32_t Step, uint32_t Lower) {
  uint32_t Index;

  if (Step == 833) {
    const uint32_t Delta = Upper - Lower;
    uint32_t Base = (Delta / 2500) * 2500;
    const uint32_t Index = ((Delta - Base) % 2500) / 833;

    if (Index == 2) {
      Base++;
    }

    return Lower + Base + (Index * 833);
  }

  Index = (Upper - Lower) / Step;

  return Lower + (Step * Index);
}

bool IsTXAllowed(uint32_t Frequency) {
  if (gSetting_ALL_TX == 2) {
    return false;
  }

  if (gSetting_ALL_TX == 1) {
    return true;
  }

  switch (gSetting_F_LOCK) {
  case F_LOCK_FCC:
    return (Frequency >= 14400000 && Frequency <= 14799990) ||
           (Frequency >= 42000000 && Frequency <= 44999990);

  case F_LOCK_CE:
    return Frequency >= 14400000 && Frequency <= 14599990;

  case F_LOCK_GB:
    return (Frequency >= 14400000 && Frequency <= 14799990) ||
           (Frequency >= 43000000 && Frequency <= 43999990);

  case F_LOCK_LPD_PMR:
    return (Frequency >= 43300000 && Frequency <= 43499990) ||
           (Frequency >= 44600000 && Frequency <= 44619990);

  default:
    return (Frequency >= 13600000 && Frequency <= 17399990) ||
           (Frequency >= 40000000 && Frequency <= 46999990) ||
           (gSetting_350TX && Frequency >= 35000000 && Frequency <= 39999990) ||
           (gSetting_200TX && Frequency >= 17400000 && Frequency <= 34999990) ||
           (gSetting_500TX && Frequency >= 47000000 && Frequency <= 60000000);
  }

  return false;
}

bool FREQUENCY_Check(VFO_Info_t *pInfo) {
  if (pInfo->CHANNEL_SAVE > FREQ_CHANNEL_LAST) {
    return false;
  }

  return IsTXAllowed(pInfo->pTX->Frequency);
}

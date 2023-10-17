/* Copyright 2023 fagci
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

#include "../app/spectrum.h"
#include <string.h>

#define F_MIN FrequencyBandTable[0].lower
#define F_MAX FrequencyBandTable[ARRAY_SIZE(FrequencyBandTable) - 1].upper

const uint16_t RSSI_MAX_VALUE = 65535;

static uint32_t initialFreq;
static char String[32];

bool isInitialized = false;
bool monitorMode = false;
bool redrawStatus = true;
bool redrawScreen = false;
bool newScanStart = true;
bool preventKeypress = true;

bool isListening = false;
bool isTransmitting = false;

State currentState = SPECTRUM, previousState = SPECTRUM;

PeakInfo peak;
ScanInfo scanInfo;
KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0};

const char *bwOptions[] = {"  25k", "12.5k", "6.25k"};
const char *modulationTypeOptions[] = {" FM", " AM", "USB"};
const uint8_t modulationTypeTuneSteps[] = {100, 50, 10};
const uint8_t modTypeReg47Values[] = {1, 7, 5};

SpectrumSettings settings = {
    .stepsCount = STEPS_64,
    .scanStepIndex = S_STEP_25_0kHz,
    .frequencyChangeStep = 80000,
    .rssiTriggerLevel = 150,
    .backlightState = true,
    .bw = BK4819_FILTER_BW_WIDE,
    .listenBw = BK4819_FILTER_BW_WIDE,
    .modulationType = false,
};

uint32_t fMeasure = 0;
uint32_t fTx = 0;
uint32_t currentFreq, tempFreq;
uint16_t rssiHistory[128] = {0};
bool blacklist[128] = {false};

static const RegisterSpec afcDisableRegSpec = {"AFC Disable", 0x73, 4, 1, 1};
static const RegisterSpec afOutRegSpec = {"AF Output Select", 0x47, 8, 0xF, 1};
static const RegisterSpec afDacGainRegSpec = {"AF DAC Gain", 0x48, 0, 0xF, 1};

static const RegisterSpec registerSpecs[] = {
    {},
    {"LNAs", 0x13, 8, 0b11, 1},
    {"LNA", 0x13, 5, 0b111, 1},
    {"PGA", 0x13, 0, 0b111, 1},
    {"MIX", 0x13, 3, 0b11, 1},

    {"IF", 0x3D, 0, 0xFFFF, 100},
    {"DEV", 0x40, 0, 0xFFF, 10},
    {"CMP", 0x31, 3, 1, 1},
    {"MIC", 0x7D, 0, 0xF, 1},
};

#ifdef ENABLE_ALL_REGISTERS
static const RegisterSpec hiddenRegisterSpecs[] = {
    {},
    {"Freq Scan Indicator", 0x0D, 15, 1, 1},
    {"F Scan High 16 bits", 0x0D, 0, 0xFFFF, 1},
    {"F Scan Low 16 bits", 0x0E, 0, 0xFFFF, 1},

    {"AGC fix", 0x7E, 15, 0b1, 1},
    {"AGC idx", 0x7E, 12, 0b111, 1},
    {"49", 0x49, 0, 0xFFFF, 100},
    {"7B", 0x7B, 0, 0xFFFF, 100},
    {"RFfiltBW1.7-4.5khz ", 0x43, 12, 0b111, 1},
    {"RFfiltBWweak1.7-4.5khz", 0x43, 9, 0b111, 1},
    {"BW Mode Selection", 0x43, 4, 0b11, 1},
    {"rssi_rel", 0x65, 8, 0xFF, 1},
    {"agc_rssi", 0x62, 8, 0xFF, 1},
    {"lna_peak_rssi", 0x62, 0, 0xFF, 1},
    {"rssi_sq", 0x67, 0, 0xFF, 1},
    {"weak_rssi 1\0", 0x0C, 7, 1, 1},
    {"ext_lna_gain set", 0x2C, 0, 0b11111, 1},
    {"snr_out", 0x61, 8, 0xFF, 1},
    {"noise sq", 0x65, 0, 0xFF, 1},
    {"glitch", 0x63, 0, 0xFF, 1},

    {"soft_mute_en 1", 0x20, 12, 1, 1},
    {"SNR Threshold SoftMut", 0x20, 0, 0b111111, 1},
    {"soft_mute_atten", 0x20, 6, 0b11, 1},
    {"soft_mute_rate", 0x20, 8, 0b11, 1},

    {"IF step100x", 0x3D, 0, 0xFFFF, 100},
    {"IF step1x", 0x3D, 0, 0xFFFF, 1},
    {"Band Selection Thr", 0x3E, 0, 0xFFFF, 100},

    {"chip_id", 0x00, 0, 0xFFFF, 1},
    {"rev_id", 0x01, 0, 0xFFFF, 1},

    {"aerror_en 0am 1fm", 0x30, 9, 1, 1},
    {"bypass 1tx 0rx", 0x47, 0, 1, 1},
    {"bypass tx gain 1", 0x47, 1, 1, 1},
    {"bps afdac 3tx 9rx ", 0x47, 8, 0b1111, 1},
    {"bps tx dcc=0 ", 0x7E, 3, 0b111, 1},

    {"audio_tx_mute1", 0x50, 15, 1, 1},
    {"audio_tx_limit_bypass1", 0x50, 10, 1, 1},
    {"audio_tx_limit320", 0x50, 0, 0x3FF, 1},
    {"audio_tx_limit reserved7", 0x50, 11, 0b1111, 1},

    {"audio_tx_path_sel", 0x2D, 2, 0b11, 1},

    {"AFTx Filt Bypass All", 0x47, 0, 1, 1},
    {"3kHz AF Resp K Tx", 0x74, 0, 0xFFFF, 100},
    {"MIC Sensit Tuning", 0x7D, 0, 0b11111, 1},
    {"DCFiltBWTxMICIn15-480hz", 0x7E, 3, 0b111, 1},
    afOutRegSpec,
    {"04 768", 0x04, 0, 0x0300, 1},
    {"43 32264", 0x43, 0, 0x7E08, 1},
    afDacGainRegSpec,
    {"4b 58434", 0x4b, 0, 0xE442, 1},
    {"73 22170", 0x73, 0, 0x569A, 1},
    {"7E 13342", 0x7E, 0, 0x341E, 1},
    {"47 26432 24896", 0x47, 0, 0x6740, 1},
    {"03 49662 49137", 0x30, 0, 0xC1FE, 1},

    {"Enable Compander", 0x31, 3, 1, 1},
    {"Band-Gap Enable", 0x37, 0, 1, 1},
    {"IF step100x", 0x3D, 0, 0xFFFF, 100},
    {"IF step1x", 0x3D, 0, 0xFFFF, 1},
    {"Band Selection Thr", 0x3E, 0, 0xFFFF, 1},
    {"RF filt BW ", 0x43, 12, 0b111, 1},
    {"RF filt BW weak", 0x43, 9, 0b111, 1},
    {"BW Mode Selection", 0x43, 4, 0b11, 1},
    {"AF Output Inverse", 0x47, 13, 1, 1},

    {"AF ALC Disable", 0x4B, 5, 1, 1},
    {"AFC Range Select", 0x73, 11, 0b111, 1},
    afcDisableRegSpec,
    {"AGC Fix Mode", 0x7E, 15, 1, 1},
    {"AGC Fix Index", 0x7E, 12, 0b111, 1},

    /*   {"LNAs 10", 0x10, 8, 0b11, 1},
       {"LNA 10", 0x10, 5, 0b111, 1},
       {"MIX 10", 0x10, 3, 0b11, 1},
       {"PGA 10", 0x10, 0, 0b111, 1},
       {"LNAs 11", 0x11, 8, 0b11, 1},
       {"LNA 11", 0x11, 5, 0b111, 1},
       {"MIX 11", 0x11, 3, 0b11, 1},
       {"PGA 11", 0x11, 0, 0b111, 1},
       {"LNAs 12", 0x12, 8, 0b11, 1},
       {"LNA 12", 0x12, 5, 0b111, 1},
       {"MIX 12", 0x12, 3, 0b11, 1},
       {"PGA 12", 0x12, 0, 0b111, 1},
       {"LNAs 13", 0x13, 8, 0b11, 1},
       {"LNA 13", 0x13, 5, 0b111, 1},
       {"MIX 13", 0x13, 3, 0b11, 1},
       {"PGA 13", 0x13, 0, 0b111, 1},
       {"LNAs 14", 0x14, 8, 0b11, 1},
       {"LNA 14", 0x14, 5, 0b111, 1},
       {"MIX 14", 0x14, 3, 0b11, 1},
       {"PGA 14", 0x14, 0, 0b111, 1},
   */
    {"Crystal vReg Bit", 0x1A, 12, 0b1111, 1},
    {"Crystal iBit", 0x1A, 8, 0b1111, 1},
    {"PLL CP bit", 0x1F, 0, 0b1111, 1},
    {"PLL/VCO Enable", 0x30, 4, 0xF, 1},
    {"XTAL Enable", 0x37, 1, 1, 1},
    {"XTAL F Low-16bits", 0x3B, 0, 0xFFFF, 1},
    {"XTAL F High-8bits", 0x3C, 8, 0xFF, 1},
    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},
    {"XTAL F reserved flt", 0x3C, 0, 0b111111, 1},
    {"Exp AF Rx Ratio", 0x28, 14, 0b11, 1},
    {"Exp AF Rx 0 dB", 0x28, 7, 0x7F, 1},
    {"Exp AF Rx noise", 0x28, 0, 0x7F, 1},
    {"OFF AFRxHPF300 flt", 0x2B, 10, 1, 1},
    {"OFF AF RxLPF3K flt", 0x2B, 9, 1, 1},
    {"OFF AF Rx de-emp", 0x2B, 8, 1, 1},
    {"Gain after FM Demod", 0x43, 2, 1, 1},
    {"AF Rx Gain1", 0x48, 10, 0x11, 1},
    {"AF Rx Gain2", 0x48, 4, 0b111111, 1},
    {"AF DAC G after G1 G2", 0x48, 0, 0b1111, 1},
    {"300Hz AF Resp K Rx", 0x54, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Rx", 0x55, 0, 0xFFFF, 100},
    {"3kHz AF Resp K Rx", 0x75, 0, 0xFFFF, 100},
    {"DC Filt BW Rx IF In", 0x7E, 0, 0b111, 1},

    {"MIC AGC Disable", 0x19, 15, 1, 1},
    {"Compress AF Tx Ratio", 0x29, 14, 0b11, 1},
    {"Compress AF Tx 0 dB", 0x29, 7, 0x7F, 1},
    {"Compress AF Tx noise", 0x29, 0, 0x7F, 1},
    {"OFF AFTxHPF300filter", 0x2B, 2, 1, 1},
    {"OFF AFTxLPF1filter", 0x2B, 1, 1, 1},
    {"OFF AFTxpre-emp flt", 0x2B, 0, 1, 1},
    {"PA Gain Enable", 0x30, 3, 1, 1},
    {"PA Biasoutput 0~3", 0x36, 8, 0xFF, 1},
    {"PA Gain1 Tuning", 0x36, 3, 0b111, 1},
    {"PA Gain2 Tuning", 0x36, 0, 0b111, 1},
    {"RF TxDeviation ON", 0x40, 12, 1, 1},
    {"RF Tx Deviation", 0x40, 0, 0xFFF, 10},
    {"AFTxLPF2fltBW1.7-4.5khz", 0x43, 6, 0b111, 1},
    {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 100},

    /*	{"REG03 en af for afout3", 0x03, 9, 1, 1},
            {"tx mute dtmf REG_50", 0x50, 15, 1, 1},
            {"tx ctcss en REG_51", 0x51, 15, 1, 1},
            {"tx dsp en REG_30", 0x30, 1, 1, 1},
            {"disc mode dis reg30", 0x30, 8, 1, 1},
            */

};
uint8_t hiddenMenuState = 0;
#endif

static uint16_t registersBackup[128];
static const uint8_t registersToBackup[] = {
    0x13, 0x30, 0x31, 0x37, 0x3D, 0x40, 0x43, 0x47, 0x48, 0x7D, 0x7E,
};

static MovingAverage mov = {{128}, {}, 255, 128, 0, 0};
static const uint8_t MOV_N = ARRAY_SIZE(mov.buf);

const uint8_t FREQ_INPUT_LENGTH = 10;
uint8_t freqInputIndex = 0;
uint8_t freqInputDotIndex = 0;
KEY_Code_t freqInputArr[10];
char freqInputString[] = "----------"; // XXXX.XXXXX

uint8_t menuState = 0;

uint16_t listenT = 0;

uint16_t batteryUpdateTimer = 0;
bool isMovingInitialized = false;
uint8_t lastStepsCount = 0;

VfoState_t txAllowState;

static uint16_t GetRegValue(RegisterSpec s) {
  return (BK4819_ReadRegister(s.num) >> s.offset) & s.mask;
}

static void SetRegValue(RegisterSpec s, uint16_t v) {
  uint16_t reg = BK4819_ReadRegister(s.num);
  reg &= ~(s.mask << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
}

static void UpdateRegMenuValue(RegisterSpec s, bool add) {
  uint16_t v = GetRegValue(s);

  if (add && v <= s.mask - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }

  SetRegValue(s, v);
  redrawScreen = true;
}

// Utility functions

KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  if (btn == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
    btn = KEY_PTT;
  }
  return btn;
}

void SetState(State state) {
  previousState = currentState;
  currentState = state;
  redrawScreen = true;
  redrawStatus = true;
}

// Radio functions

static void BackupRegisters() {
  for (uint8_t i = 0; i < ARRAY_SIZE(registersToBackup); ++i) {
    uint8_t regNum = registersToBackup[i];
    registersBackup[regNum] = BK4819_ReadRegister(regNum);
  }
}

static void RestoreRegisters() {
  for (uint8_t i = 0; i < ARRAY_SIZE(registersToBackup); ++i) {
    uint8_t regNum = registersToBackup[i];
    BK4819_WriteRegister(regNum, registersBackup[regNum]);
  }
}

static void SetModulation(ModulationType type) {
  // restore only registers, which we affect here fully
  BK4819_WriteRegister(0x37, registersBackup[0x37]);
  BK4819_WriteRegister(0x3D, registersBackup[0x3D]);
  BK4819_WriteRegister(0x48, registersBackup[0x48]);

  SetRegValue(afOutRegSpec, modTypeReg47Values[type]);

  if (type == MOD_USB) {
    BK4819_WriteRegister(0x37, 0b0001011000001111);
    BK4819_WriteRegister(0x3D, 0b0010101101000101);
    BK4819_WriteRegister(0x48, 0b0000001110101000);
  } else if (type == MOD_AM) {
    SetRegValue(afDacGainRegSpec, 0xE);
  }
  SetRegValue(afcDisableRegSpec,
              settings.modulationType != MOD_FM); // disable AFC if not FM
}

static void ApplyFreqChange() {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, reg & ~BK4819_REG_30_ENABLE_VCO_CALIB);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

static void Set1ARegBasedOnF(uint32_t f) {
  uint16_t v;
  if (f >= 74000000) {
    v = 0x1f80;
  } else if (f >= 37000000) {
    v = 0x2f50;
  } else if (f >= 24700000) {
    v = 0x9f30;
  } else if (f >= 18500000) {
    v = 0x3f48;
  } else if (f >= 12400000) {
    v = 0xaf28;
  } else if (f >= 9300000) {
    v = 0x4f44;
  } else if (f >= 6200000) {
    v = 0xbf24;
  } else if (f >= 4600000) {
    v = 0x5f42;
  } else if (f >= 3100000) {
    v = 0xcf22;
  } else if (f >= 2300000) {
    v = 0x6f41;
  } else {
    v = 0xdf21;
  }
  BK4819_WriteRegister(0x1A, v);
}

static void SetF(uint32_t f) {
  if (fMeasure == f) {
    return;
  }
  fMeasure = f;
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  Set1ARegBasedOnF(f);
  BK4819_SetFrequency(f);
  ApplyFreqChange();
}

static void SetTxF(uint32_t f) {
  fTx = f;
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  Set1ARegBasedOnF(f);
  BK4819_SetFrequency(f);
  ApplyFreqChange();
}

// Spectrum related

bool IsPeakOverLevel() { return peak.rssi >= settings.rssiTriggerLevel; }

static void ResetPeak() {
  peak.t = 0;
  peak.rssi = 0;
}

bool IsCenterMode() { return settings.scanStepIndex < S_STEP_1_0kHz; }
uint8_t GetStepsCount() { return 128 >> settings.stepsCount; }
uint16_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }
uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }
uint32_t GetFStart() {
  return IsCenterMode() ? currentFreq - (GetBW() >> 1) : currentFreq;
}
uint32_t GetFEnd() { return currentFreq + GetBW(); }

static void MovingCp(uint16_t *dst, uint16_t *src) {
  memcpy(dst, src, GetStepsCount() * sizeof(uint16_t));
}

static void ResetMoving() {
  for (uint8_t i = 0; i < MOV_N; ++i) {
    MovingCp(mov.buf[i], rssiHistory);
  }
}

static void MoveHistory() {
  const uint8_t XN = GetStepsCount();

  uint32_t midSum = 0;

  mov.min = RSSI_MAX_VALUE;
  mov.max = 0;

  if (lastStepsCount != XN) {
    ResetMoving();
    lastStepsCount = XN;
  }
  for (uint8_t i = MOV_N - 1; i > 0; --i) {
    MovingCp(mov.buf[i], mov.buf[i - 1]);
  }
  MovingCp(mov.buf[0], rssiHistory);

  uint8_t skipped = 0;

  for (uint8_t x = 0; x < XN; ++x) {
    if (blacklist[x]) {
      skipped++;
      continue;
    }
    uint32_t sum = 0;
    for (uint8_t i = 0; i < MOV_N; ++i) {
      sum += mov.buf[i][x];
    }

    uint16_t pointV = mov.mean[x] = sum / MOV_N;

    midSum += pointV;

    if (pointV > mov.max) {
      mov.max = pointV;
    }

    if (pointV < mov.min) {
      mov.min = pointV;
    }
  }
  if (skipped == XN) {
    return;
  }

  mov.mid = midSum / (XN - skipped);
}

static void TuneToPeak() {
  scanInfo.f = peak.f;
  scanInfo.rssi = peak.rssi;
  scanInfo.i = peak.i;
  SetF(scanInfo.f);
}

uint8_t GetBWRegValueForScan() {
  return scanStepBWRegValues[settings.scanStepIndex == S_STEP_100_0kHz ? 11
                                                                       : 0];
}

uint8_t GetBWRegValueForListen() {
  return listenBWRegValues[settings.listenBw];
}

static void ResetRSSI() {
  uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
  Reg |= 1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

uint16_t GetRssi() {
  if (currentState == SPECTRUM) {
    if (0)
      ResetRSSI();
    SYSTEM_DelayMs(10);
  }
  return BK4819_GetRSSI();
}

uint32_t GetOffsetedF(uint32_t f) {
  switch (gCurrentVfo->FREQUENCY_DEVIATION_SETTING) {
  case FREQUENCY_DEVIATION_OFF:
    break;
  case FREQUENCY_DEVIATION_ADD:
    f += gCurrentVfo->FREQUENCY_OF_DEVIATION;
    break;
  case FREQUENCY_DEVIATION_SUB:
    f -= gCurrentVfo->FREQUENCY_OF_DEVIATION;
    break;
  }

  return Clamp(f, FrequencyBandTable[0].lower,
               FrequencyBandTable[ARRAY_SIZE(FrequencyBandTable) - 1].upper);
}

static void ToggleAudio(bool on) {
  if (on) {
    GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  } else {
    GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  }
}

static void ToggleTX(bool);
static void ToggleRX(bool);

static void ToggleRX(bool on) {
  if (isListening == on) {
    return;
  }
  isListening = on;
  if (on) {
    ToggleTX(false);
  }

  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, on);
  BK4819_RX_TurnOn();

  ToggleAudio(on);
  BK4819_ToggleAFDAC(on);
  BK4819_ToggleAFBit(on);

  if (on) {
    listenT = 1000;
    BK4819_WriteRegister(0x43, GetBWRegValueForListen());
  } else {
    BK4819_WriteRegister(0x43, GetBWRegValueForScan());
  }
}

uint16_t registersVault[128] = {0};

static void RegBackupSet(uint8_t num, uint16_t value) {
  registersVault[num] = BK4819_ReadRegister(num);
  BK4819_WriteRegister(num, value);
}

static void RegRestore(uint8_t num) {
  BK4819_WriteRegister(num, registersVault[num]);
}

static void ToggleTX(bool on) {
  if (isTransmitting == on) {
    return;
  }
  isTransmitting = on;
  if (on) {
    ToggleRX(false);
  }

  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_RED, on);

  if (on) {
    ToggleAudio(false);

    SetTxF(GetOffsetedF(fMeasure));

    RegBackupSet(BK4819_REG_47, 0x6040);
    RegBackupSet(BK4819_REG_7E, 0x302E);
    RegBackupSet(BK4819_REG_50, 0x3B20);
    RegBackupSet(BK4819_REG_37, 0x1D0F);
    RegBackupSet(BK4819_REG_52, 0x028F);
    RegBackupSet(BK4819_REG_30, 0x0000);
    BK4819_WriteRegister(BK4819_REG_30, 0xC1FE);
    RegBackupSet(BK4819_REG_51, 0x0000);

    BK4819_SetupPowerAmplifier(gCurrentVfo->TXP_CalculatedSetting,
                               gCurrentVfo->pTX->Frequency);
  } else {
    RADIO_SendEndOfTransmission();
    RADIO_EnableCxCSS();

    BK4819_SetupPowerAmplifier(0, 0);

    RegRestore(BK4819_REG_51);
    BK4819_WriteRegister(BK4819_REG_30, 0);
    RegRestore(BK4819_REG_30);
    RegRestore(BK4819_REG_52);
    RegRestore(BK4819_REG_37);
    RegRestore(BK4819_REG_50);
    RegRestore(BK4819_REG_7E);
    RegRestore(BK4819_REG_47);

    SetF(fMeasure);
  }
  BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2, !on);
  BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1, on);
}

// Scan info

static void ResetScanStats() {
  scanInfo.rssi = 0;
  scanInfo.rssiMax = 0;
  scanInfo.iPeak = 0;
  scanInfo.fPeak = 0;
}

static void InitScan() {
  ResetScanStats();
  scanInfo.i = 0;
  scanInfo.f = GetFStart();

  scanInfo.scanStep = GetScanStep();
  scanInfo.measurementsCount = GetStepsCount();
}

static void ResetBlacklist() { memset(blacklist, false, 128); }

static void RelaunchScan() {
  InitScan();
  ResetPeak();
  lastStepsCount = 0;
  ToggleRX(false);
#ifdef SPECTRUM_AUTOMATIC_SQUELCH
  settings.rssiTriggerLevel = RSSI_MAX_VALUE;
#endif
  preventKeypress = true;
  scanInfo.rssiMin = RSSI_MAX_VALUE;
  redrawStatus = true;
}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
    scanInfo.fPeak = scanInfo.f;
    scanInfo.iPeak = scanInfo.i;
  }

  if (scanInfo.rssi < scanInfo.rssiMin) {
    scanInfo.rssiMin = scanInfo.rssi;
  }
}

static void AutoTriggerLevel() {
  if (settings.rssiTriggerLevel == RSSI_MAX_VALUE) {
    settings.rssiTriggerLevel = Clamp(scanInfo.rssiMax + 4, 0, RSSI_MAX_VALUE);
  }
}

static void UpdatePeakInfoForce() {
  peak.t = 0;
  peak.rssi = scanInfo.rssiMax;
  peak.f = scanInfo.fPeak;
  peak.i = scanInfo.iPeak;
  AutoTriggerLevel();
}

static void UpdatePeakInfo() {
  if (peak.f == 0 || peak.t >= 1024 || peak.rssi < scanInfo.rssiMax)
    UpdatePeakInfoForce();
}

static void Measure() {
  // rm harmonics using blacklist for now
  if (scanInfo.f % 1300000 == 0) {
    blacklist[scanInfo.i] = true;
    return;
  }
  rssiHistory[scanInfo.i] = scanInfo.rssi = GetRssi();
}

// Update things by keypress

static void UpdateRssiTriggerLevel(bool inc) {
  if (inc)
    settings.rssiTriggerLevel += 2;
  else
    settings.rssiTriggerLevel -= 2;
  redrawScreen = true;
  SYSTEM_DelayMs(10);
}

static void ApplyPreset(FreqPreset p) {
  currentFreq = p.fStart;
  settings.scanStepIndex = p.stepSizeIndex;
  settings.listenBw = p.listenBW;
  settings.modulationType = p.modulationType;
  settings.stepsCount = p.stepsCountIndex;
  SetModulation(settings.modulationType);
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
  settings.frequencyChangeStep = GetBW();
}

static void SelectNearestPreset(bool inc) {
  FreqPreset p;
  const uint8_t SZ = ARRAY_SIZE(freqPresets);
  if (inc) {
    for (uint8_t i = 0; i < SZ; ++i) {
      p = freqPresets[i];
      if (currentFreq < p.fStart) {
        ApplyPreset(p);
        return;
      }
    }
  } else {
    for (int i = SZ - 1; i >= 0; --i) {
      p = freqPresets[i];
      if (currentFreq > p.fEnd) {
        ApplyPreset(p);
        return;
      }
    }
  }
  ApplyPreset(p);
}

static void UpdateScanStep(bool inc) {
  if (inc && settings.scanStepIndex < S_STEP_100_0kHz) {
    settings.scanStepIndex++;
  } else if (!inc && settings.scanStepIndex > 0) {
    settings.scanStepIndex--;
  } else {
    return;
  }
  settings.frequencyChangeStep = GetBW() >> 1;
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
}

static void UpdateCurrentFreq(bool inc) {
  if (inc && currentFreq < F_MAX) {
    currentFreq += settings.frequencyChangeStep;
  } else if (!inc && currentFreq > F_MIN) {
    currentFreq -= settings.frequencyChangeStep;
  } else {
    return;
  }
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
}

static void UpdateCurrentFreqStill(bool inc) {
  uint8_t offset = modulationTypeTuneSteps[settings.modulationType];
  uint32_t f = fMeasure;
  if (inc && f < F_MAX) {
    f += offset;
  } else if (!inc && f > F_MIN) {
    f -= offset;
  }
  SetF(f);
  SYSTEM_DelayMs(10);
  redrawScreen = true;
}

static void UpdateFreqChangeStep(bool inc) {
  uint16_t diff = GetScanStep() * 4;
  if (inc && settings.frequencyChangeStep < 1280000) {
    settings.frequencyChangeStep += diff;
  } else if (!inc && settings.frequencyChangeStep > 10000) {
    settings.frequencyChangeStep -= diff;
  }
  SYSTEM_DelayMs(100);
  redrawScreen = true;
}

static void ToggleModulation() {
  if (settings.modulationType < MOD_USB) {
    settings.modulationType++;
  } else {
    settings.modulationType = MOD_FM;
  }
  SetModulation(settings.modulationType);
  redrawScreen = true;
}

static void ToggleListeningBW() {
  if (settings.listenBw == BK4819_FILTER_BW_NARROWER) {
    settings.listenBw = BK4819_FILTER_BW_WIDE;
  } else {
    settings.listenBw++;
  }
  redrawScreen = true;
}

static void ToggleBacklight() {
  settings.backlightState = !settings.backlightState;
  if (settings.backlightState) {
    GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
  } else {
    GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
  }
}

static void ToggleStepsCount() {
  if (settings.stepsCount == STEPS_128) {
    settings.stepsCount = STEPS_16;
  } else {
    settings.stepsCount--;
  }
  settings.frequencyChangeStep = GetBW() >> 1;
  RelaunchScan();
  ResetBlacklist();
  redrawScreen = true;
}

static void ResetFreqInput() {
  tempFreq = 0;
  memset(freqInputString, '-', 10);
}

static void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
  SetState(FREQ_INPUT);
}

static void UpdateFreqInput(KEY_Code_t key) {
  if (key != KEY_EXIT && freqInputIndex >= 10) {
    return;
  }
  if (key == KEY_STAR) {
    if (freqInputIndex == 0 || freqInputDotIndex) {
      return;
    }
    freqInputDotIndex = freqInputIndex;
  }
  if (key == KEY_EXIT) {
    freqInputIndex--;
  } else {
    freqInputArr[freqInputIndex++] = key;
  }

  ResetFreqInput();

  uint8_t dotIndex =
      freqInputDotIndex == 0 ? freqInputIndex : freqInputDotIndex;

  KEY_Code_t digitKey;
  for (uint8_t i = 0; i < 10; ++i) {
    if (i < freqInputIndex) {
      digitKey = freqInputArr[i];
      freqInputString[i] = digitKey <= KEY_9 ? '0' + digitKey : '.';
    } else {
      freqInputString[i] = '-';
    }
  }

  uint32_t base = 100000; // 1MHz in BK units
  for (int i = dotIndex - 1; i >= 0; --i) {
    tempFreq += freqInputArr[i] * base;
    base *= 10;
  }

  base = 10000; // 0.1MHz in BK units
  if (dotIndex < freqInputIndex) {
    for (uint8_t i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += freqInputArr[i] * base;
      base /= 10;
    }
  }
  redrawScreen = true;
}

static void Blacklist() {
  blacklist[peak.i] = true;
  ResetPeak();
  ToggleRX(false);
  newScanStart = true;
  redrawScreen = true;
}

// Draw things

static uint8_t Rssi2Y(uint16_t rssi) {
  return DrawingEndY - ConvertDomain(rssi, mov.min - 2,
                                     mov.max + 30 + (mov.max - mov.min) / 3, 0,
                                     DrawingEndY);
}

static void DrawSpectrum() {
  for (uint8_t x = 0; x < 128; ++x) {
    uint8_t i = x >> settings.stepsCount;
    if (blacklist[i]) {
      continue;
    }
    uint16_t rssi = rssiHistory[i];
    DrawHLine(Rssi2Y(rssi), DrawingEndY, x, true);
  }
}

static void UpdateBatteryInfo() {
  for (uint8_t i = 0; i < 4; i++) {
    BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i], &gBatteryCurrent);
  }

  uint16_t voltage = Mid(gBatteryVoltages, ARRAY_SIZE(gBatteryVoltages));
  gBatteryDisplayLevel = 0;

  for (int i = ARRAY_SIZE(gBatteryCalibration) - 1; i >= 0; --i) {
    if (gBatteryCalibration[i] < voltage) {
      gBatteryDisplayLevel = i + 1;
      break;
    }
  }
}

static void DrawStatus() {

  if (currentState == SPECTRUM) {
    const FreqPreset *p = NULL;
    for (uint8_t i = 0; i < ARRAY_SIZE(freqPresets); ++i) {
      if (currentFreq >= freqPresets[i].fStart &&
          currentFreq < freqPresets[i].fEnd) {
        p = &freqPresets[i];
      }
    }
    if (p != NULL) {
      UI_PrintStringSmallest(p->name, 0, 1, true, true);
    }
  }

  if (currentState == STILL) {
    sprintf(String, "AFC %s", GetRegValue(afcDisableRegSpec) ? "OFF" : "ON");
    UI_PrintStringSmallest(String, 0, 1, true, true);
  }

  gStatusLine[127] = 0b01111110;
  for (uint8_t i = 126; i >= 116; i--) {
    gStatusLine[i] = 0b01000010;
  }
  uint8_t v = gBatteryDisplayLevel;
  v <<= 1;
  for (uint8_t i = 125; i >= 116; i--) {
    if (126 - i <= v) {
      gStatusLine[i + 2] = 0b01111110;
    }
  }
  gStatusLine[117] = 0b01111110;
  gStatusLine[116] = 0b00011000;
}

static void DrawF(uint32_t f) {
  sprintf(String, "%s", modulationTypeOptions[settings.modulationType]);
  UI_PrintStringSmallest(String, 116, 1, false, true);
  sprintf(String, "%s", bwOptions[settings.listenBw]);
  UI_PrintStringSmallest(String, 108, 7, false, true);

  if (currentState == SPECTRUM && !f) {
    return;
  }

  sprintf(String, "%u.%05u", f / 100000, f % 100000);

  if (currentState == STILL && kbd.current == KEY_PTT) {
    switch (txAllowState) {
    case VFO_STATE_NORMAL:
      if (isTransmitting) {
        f = GetOffsetedF(f);
        sprintf(String, "TX %u.%05u", f / 100000, f % 100000);
      }
      break;
    case VFO_STATE_VOL_HIGH:
      sprintf(String, "VOLTAGE HIGH");
      break;
    default:
      sprintf(String, "DISABLED");
    }
  }
  UI_PrintStringSmall(String, 8, 127, 0);
}

static void DrawNums() {
  if (currentState == SPECTRUM) {
    sprintf(String, "%ux", GetStepsCount());
    UI_PrintStringSmallest(String, 0, 1, false, true);
    sprintf(String, "%u.%02uk", GetScanStep() / 100, GetScanStep() % 100);
    UI_PrintStringSmallest(String, 0, 7, false, true);
  }

  if (IsCenterMode()) {
    sprintf(String, "%u.%05u \xB1%u.%02uk", currentFreq / 100000,
            currentFreq % 100000, settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    UI_PrintStringSmallest(String, 36, 49, false, true);
  } else {
    sprintf(String, "%u.%05u", GetFStart() / 100000, GetFStart() % 100000);
    UI_PrintStringSmallest(String, 0, 49, false, true);

    sprintf(String, "\xB1%uk", settings.frequencyChangeStep / 100);
    UI_PrintStringSmallest(String, 52, 49, false, true);

    sprintf(String, "%u.%05u", GetFEnd() / 100000, GetFEnd() % 100000);
    UI_PrintStringSmallest(String, 93, 49, false, true);
  }
}

static void DrawRssiTriggerLevel() {
  if (settings.rssiTriggerLevel == RSSI_MAX_VALUE || monitorMode)
    return;
  uint8_t y = Rssi2Y(settings.rssiTriggerLevel);
  for (uint8_t x = 0; x < 128; x += 2) {
    PutPixel(x, y, true);
  }
}

static void DrawTicks() {
  uint32_t f = GetFStart() % 100000;
  uint32_t step = GetScanStep();
  for (uint8_t i = 0; i < 128; i += (1 << settings.stepsCount), f += step) {
    uint8_t barValue = 0b00000001;
    (f % 10000) < step && (barValue |= 0b00000010);
    (f % 50000) < step && (barValue |= 0b00000100);
    (f % 100000) < step && (barValue |= 0b00011000);

    gFrameBuffer[5][i] |= barValue;
  }

  // center
  /* if (IsCenterMode()) {
    gFrameBuffer[5][62] = 0x80;
    gFrameBuffer[5][63] = 0x80;
    gFrameBuffer[5][64] = 0xff;
    gFrameBuffer[5][65] = 0x80;
    gFrameBuffer[5][66] = 0x80;
  } else {
    gFrameBuffer[5][0] = 0xff;
    gFrameBuffer[5][1] = 0x80;
    gFrameBuffer[5][2] = 0x80;
    gFrameBuffer[5][3] = 0x80;
    gFrameBuffer[5][124] = 0x80;
    gFrameBuffer[5][125] = 0x80;
    gFrameBuffer[5][126] = 0x80;
    gFrameBuffer[5][127] = 0xff;
  } */
}

static void DrawArrow(uint8_t x) {
  for (signed i = -2; i <= 2; ++i) {
    signed v = x + i;
    uint8_t a = i > 0 ? i : -i;
    if (!(v & 128)) {
      gFrameBuffer[5][v] |= (0b01111000 << a) & 0b01111000;
    }
  }
}

static void DeInitSpectrum() {
  SetF(initialFreq);
  ToggleRX(false);
  RestoreRegisters();
  isInitialized = false;
}

static void OnKeyDown(uint8_t key) {
  switch (key) {
  case KEY_3:
    SelectNearestPreset(true);
    break;
  case KEY_9:
    SelectNearestPreset(false);
    break;
  case KEY_1:
    UpdateScanStep(true);
    break;
  case KEY_7:
    UpdateScanStep(false);
    break;
  case KEY_2:
    UpdateFreqChangeStep(true);
    break;
  case KEY_8:
    UpdateFreqChangeStep(false);
    break;
  case KEY_UP:
    UpdateCurrentFreq(true);
    break;
  case KEY_DOWN:
    UpdateCurrentFreq(false);
    break;
  case KEY_SIDE1:
    Blacklist();
    break;
  case KEY_STAR:
    UpdateRssiTriggerLevel(true);
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(false);
    break;
  case KEY_5:
    FreqInput();
    break;
  case KEY_0:
    ToggleModulation();
    break;
  case KEY_6:
    ToggleListeningBW();
    break;
  case KEY_4:
    ToggleStepsCount();
    break;
  case KEY_SIDE2:
    ToggleBacklight();
    break;
  case KEY_PTT:
    SetState(STILL);
    TuneToPeak();
    settings.rssiTriggerLevel = 120;
    break;
  case KEY_MENU:
    break;
  case KEY_EXIT:
    if (menuState) {
      menuState = 0;
      redrawScreen = true;
      break;
    }
    DeInitSpectrum();
    break;
  default:
    break;
  }
}

static void OnKeyDownFreqInput(uint8_t key) {
  switch (key) {
  case KEY_0:
  case KEY_1:
  case KEY_2:
  case KEY_3:
  case KEY_4:
  case KEY_5:
  case KEY_6:
  case KEY_7:
  case KEY_8:
  case KEY_9:
  case KEY_STAR:
    UpdateFreqInput(key);
    break;
  case KEY_EXIT:
    if (freqInputIndex == 0) {
      SetState(previousState);
      break;
    }
    UpdateFreqInput(key);
    break;
  case KEY_MENU:
    if (tempFreq < F_MIN || tempFreq > F_MAX) {
      break;
    }
    SetState(previousState);
    currentFreq = tempFreq;
    if (currentState == SPECTRUM) {
      ResetBlacklist();
      RelaunchScan();
    } else {
      SetF(currentFreq);
    }
    break;
  default:
    break;
  }
  SYSTEM_DelayMs(90);
}

void OnKeyDownStill(KEY_Code_t key) {
  switch (key) {
#ifdef ENABLE_ALL_REGISTERS
  case KEY_2:
    menuState = 0;
    if (hiddenMenuState <= 1) {
      hiddenMenuState = ARRAY_SIZE(hiddenRegisterSpecs) - 1;
    } else {
      hiddenMenuState--;
    }
    redrawScreen = true;
    SYSTEM_DelayMs(150);
    break;
  case KEY_8:
    menuState = 0;
    if (hiddenMenuState == ARRAY_SIZE(hiddenRegisterSpecs) - 1) {
      hiddenMenuState = 1;
    } else {
      hiddenMenuState++;
    }
    redrawScreen = true;
    SYSTEM_DelayMs(150);
    break;
#endif
  case KEY_UP:
    if (menuState) {
      UpdateRegMenuValue(registerSpecs[menuState], true);
      break;
    }
#ifdef ENABLE_ALL_REGISTERS
    if (hiddenMenuState) {
      UpdateRegMenuValue(hiddenRegisterSpecs[hiddenMenuState], true);
      break;
    }
#endif
    UpdateCurrentFreqStill(true);
    break;
  case KEY_DOWN:
    if (menuState) {
      UpdateRegMenuValue(registerSpecs[menuState], false);
      break;
    }
#ifdef ENABLE_ALL_REGISTERS
    if (hiddenMenuState) {
      UpdateRegMenuValue(hiddenRegisterSpecs[hiddenMenuState], false);
      break;
    }
#endif
    UpdateCurrentFreqStill(false);
    break;
  case KEY_STAR:
    UpdateRssiTriggerLevel(true);
    break;
  case KEY_F:
    UpdateRssiTriggerLevel(false);
    break;
  case KEY_5:
    FreqInput();
    break;
  case KEY_0:
    ToggleModulation();
    break;
  case KEY_6:
    ToggleListeningBW();
    break;
  case KEY_SIDE1:
    monitorMode = !monitorMode;
    break;
  case KEY_SIDE2:
    ToggleBacklight();
    break;
  case KEY_PTT:
    // start transmit
    UpdateBatteryInfo();
    if (gBatteryDisplayLevel == 6) {
      txAllowState = VFO_STATE_VOL_HIGH;
    } else if (IsTXAllowed(GetOffsetedF(fMeasure))) {
      txAllowState = VFO_STATE_NORMAL;
      ToggleTX(true);
    } else {
      txAllowState = VFO_STATE_TX_DISABLE;
    }
    redrawScreen = true;
    break;
  case KEY_MENU:
    if (menuState == ARRAY_SIZE(registerSpecs) - 1) {
      menuState = 1;
    } else {
      menuState++;
    }
    SYSTEM_DelayMs(100);
    redrawScreen = true;
    break;
  case KEY_EXIT:
    if (menuState) {
      menuState = 0;
      redrawScreen = true;
      break;
    }
#ifdef ENABLE_ALL_REGISTERS
    if (hiddenMenuState) {
      hiddenMenuState = 0;
      redrawScreen = true;
      break;
    }
#endif
    SetState(SPECTRUM);
    monitorMode = false;
    RelaunchScan();
    break;
  default:
    break;
  }
  redrawStatus = true;
}

static void OnKeysReleased() {
  if (isTransmitting) {
    ToggleTX(false);
  }
}

static void RenderFreqInput() {
  UI_PrintString(freqInputString, 2, 127, 0, 8, true);
}

static void RenderStatus() {
  memset(gStatusLine, 0, sizeof(gStatusLine));
  DrawStatus();
  ST7565_BlitStatusLine();
}

static void RenderSpectrum() {
  DrawTicks();
  DrawArrow(peak.i << settings.stepsCount);
  DrawSpectrum();
  DrawRssiTriggerLevel();
  DrawF(peak.f);
  DrawNums();
}

static void RenderStill() {
  DrawF(fMeasure);

  const uint8_t METER_PAD_LEFT = 3;
  uint8_t *ln = gFrameBuffer[2];

  for (uint8_t i = METER_PAD_LEFT; i < 121 + METER_PAD_LEFT; i++) {
    if (i % 10 == 0) {
      ln[i] = 0b11000000;
    } else {
      ln[i] = 0b01000000;
    }
  }

  uint8_t x = Rssi2PX(scanInfo.rssi, 0, 121);
  for (uint8_t i = 0; i < x; ++i) {
    if (i % 5 && i / 5 < x / 5) {
      ln[i + METER_PAD_LEFT] |= 0b00011100;
    }
  }

  int dbm = Rssi2DBm(scanInfo.rssi);
  uint8_t s = DBm2S(dbm);
  if (s < 10) {
    sprintf(String, "S%u", s);
  } else {
    sprintf(String, "S9+%u0", s - 9);
  }
  UI_PrintStringSmallest(String, 4, 10, false, true);
  sprintf(String, "%d dBm", dbm);
  UI_PrintStringSmallest(String, 32, 10, false, true);

  if (isTransmitting) {
    uint8_t afDB = BK4819_ReadRegister(0x6F) & 0b1111111;
    uint8_t afPX = ConvertDomain(afDB, 26, 194, 0, 121);
    for (uint8_t i = 0; i < afPX; ++i) {
      gFrameBuffer[3][i + METER_PAD_LEFT] |= 0b00000011;
    }
  }

  if (!monitorMode) {
    uint8_t x = Rssi2PX(settings.rssiTriggerLevel, 0, 121);
    ln[METER_PAD_LEFT + x - 1] |= 0b01000001;
    ln[METER_PAD_LEFT + x] = 0b01111111;
    ln[METER_PAD_LEFT + x + 1] |= 0b01000001;
  }

#ifdef ENABLE_ALL_REGISTERS
  if (hiddenMenuState) {
    uint8_t hiddenMenuLen = ARRAY_SIZE(hiddenRegisterSpecs);
    uint8_t offset = Clamp(hiddenMenuState - 2, 1, hiddenMenuLen - 5);
    for (int i = 0; i < 5; ++i) {
      RegisterSpec s = hiddenRegisterSpecs[i + offset];
      bool isCurrent = hiddenMenuState == i + offset;
      sprintf(String, "%s%x %s: %u", isCurrent ? ">" : " ", s.num, s.name,
              GetRegValue(s));
      UI_PrintStringSmallest(String, 0, i * 6 + 26, false, true);
    }
  } else {
#endif
    const uint8_t PAD_LEFT = 4;
    const uint8_t CELL_WIDTH = 30;
    uint8_t offset = PAD_LEFT;
    uint8_t row = 3;

    for (int i = 0, idx = 1; idx < ARRAY_SIZE(registerSpecs); ++i, ++idx) {
      if (idx == 5) {
        row += 2;
        i = 0;
      }
      offset = PAD_LEFT + i * CELL_WIDTH;
      if (menuState == idx) {
        for (int j = 0; j < CELL_WIDTH; ++j) {
          gFrameBuffer[row][j + offset] = 0xFF;
          gFrameBuffer[row + 1][j + offset] = 0xFF;
        }
      }
      RegisterSpec s = registerSpecs[idx];
      sprintf(String, "%s", s.name);
      UI_PrintStringSmallest(String, offset + 2, row * 8 + 2, false,
                             menuState != idx);
      sprintf(String, "%u", GetRegValue(s));
      UI_PrintStringSmallest(String, offset + 2, (row + 1) * 8 + 1, false,
                             menuState != idx);
    }
#ifdef ENABLE_ALL_REGISTERS
  }
#endif
}

static void Render() {
  memset(gFrameBuffer, 0, sizeof(gFrameBuffer));

  switch (currentState) {
  case SPECTRUM:
    RenderSpectrum();
    break;
  case FREQ_INPUT:
    RenderFreqInput();
    break;
  case STILL:
    RenderStill();
    break;
  }

  ST7565_BlitFullScreen();
}

bool HandleUserInput() {
  kbd.prev = kbd.current;
  kbd.current = GetKey();

  if (kbd.current == KEY_INVALID) {
    kbd.counter = 0;
    OnKeysReleased();
    return true;
  }

  if (kbd.current == kbd.prev && kbd.counter <= 40) {
    kbd.counter++;
    SYSTEM_DelayMs(10);
  }

  if (kbd.counter == 5 || kbd.counter > 40) {
    switch (currentState) {
    case SPECTRUM:
      OnKeyDown(kbd.current);
      break;
    case FREQ_INPUT:
      OnKeyDownFreqInput(kbd.current);
      break;
    case STILL:
      OnKeyDownStill(kbd.current);
      break;
    }
  }

  return true;
}

static void Scan() {
  if (blacklist[scanInfo.i]) {
    return;
  }
  SetF(scanInfo.f);
  Measure();
  UpdateScanInfo();
}

static void NextScanStep() {
  ++peak.t;
  ++scanInfo.i;
  scanInfo.f += scanInfo.scanStep;
}

static void UpdateScan() {
  Scan();

  if (scanInfo.i < scanInfo.measurementsCount) {
    NextScanStep();
    return;
  }

  MoveHistory();

  redrawScreen = true;
  preventKeypress = false;

  UpdatePeakInfo();
  if (IsPeakOverLevel()) {
    ToggleRX(true);
    TuneToPeak();
    return;
  }

  newScanStart = true;
}
uint16_t screenRedrawT = 0;
static void UpdateStill() {
  Measure();

  peak.rssi = scanInfo.rssi;
  AutoTriggerLevel();

  if (++screenRedrawT >= 1000) {
    screenRedrawT = 0;
    redrawScreen = true;
  }

  ToggleRX(IsPeakOverLevel() || monitorMode);
}

static void UpdateListening() {
  if (!isListening) {
    ToggleRX(true);
  }
  /* if (currentState == STILL) {
    listenT = 0;
  } */
  /* if (listenT % 10 == 0) {
    AM_fix_10ms(0);
  } */
  if (listenT) {
    listenT--;
    SYSTEM_DelayMs(1);
    return;
  }

  redrawScreen = true;

  if (currentState == SPECTRUM) {
    BK4819_WriteRegister(0x43, GetBWRegValueForScan());
    Measure();
    BK4819_WriteRegister(0x43, GetBWRegValueForListen());
  } else {
    Measure();
  }

  peak.rssi = scanInfo.rssi;
  // AM_fix_reset(0);

  if (IsPeakOverLevel() || monitorMode) {
    listenT = currentState == SPECTRUM ? 1000 : 10;
    return;
  }

  ToggleRX(false);
  newScanStart = true;
}

static void UpdateTransmitting() {}

static void Tick() {
#if defined(ENABLE_UART)
  if (UART_IsCommandAvailable()) {
    __disable_irq();
    UART_HandleCommand();
    __enable_irq();
  }
#endif
  if (newScanStart) {
    InitScan();
    newScanStart = false;
  }
  if (isTransmitting) {
    UpdateTransmitting();
  } else if (isListening && currentState != FREQ_INPUT) {
    UpdateListening();
  } else {
    if (currentState == SPECTRUM) {
      UpdateScan();
    } else if (currentState == STILL) {
      UpdateStill();
    }
  }
  if (++batteryUpdateTimer > 4096) {
    batteryUpdateTimer = 0;
    UpdateBatteryInfo();
    redrawStatus = true;
  }
  if (redrawStatus) {
    RenderStatus();
    redrawStatus = false;
  }
  if (redrawScreen) {
    Render();
    redrawScreen = false;
  }
  if (!preventKeypress) {
    HandleUserInput();
  }
}

static void AutomaticPresetChoose(uint32_t f) {
  const FreqPreset *p;
  for (uint8_t i = 0; i < ARRAY_SIZE(freqPresets); ++i) {
    p = &freqPresets[i];
    if (f >= p->fStart && f <= p->fEnd) {
      ApplyPreset(*p);
    }
  }
}

void APP_RunSpectrum() {
  BackupRegisters();

  BK4819_SetAGC(1); // normalize initial gain

  // default agc table
  //      LNAs LNA MI PGA
  // 000000 00 001 11 000 :10
  // 000000 10 010 11 010 :11
  // 000000 11 011 11 011 :12
  // 000000 11 110 11 110 :13
  // 000000 00 000 00 000 :14
  /* BK4819_WriteRegister(0x10, 0b0000000000011111);
  BK4819_WriteRegister(0x11, 0b0000000000111111);
  BK4819_WriteRegister(0x12, 0b0000000001011111);
  BK4819_WriteRegister(0x13, 0b0000000001111111);
  BK4819_WriteRegister(0x14, 0b0000000010011111);
  BK4819_WriteRegister(BK4819_REG_49, 0);
  BK4819_WriteRegister(BK4819_REG_7B, 0); */

  // AM_fix_init();

  // TX here coz it always? set to active VFO
  VFO_Info_t vfo = gEeprom.VfoInfo[gEeprom.TX_CHANNEL];
  initialFreq = vfo.pRX->Frequency;
  currentFreq = initialFreq;
  settings.scanStepIndex = gStepSettingToIndex[vfo.STEP_SETTING];
  settings.listenBw = vfo.CHANNEL_BANDWIDTH == BANDWIDTH_WIDE
                          ? BANDWIDTH_WIDE
                          : BANDWIDTH_NARROW;
  settings.modulationType = vfo.IsAM ? MOD_AM : MOD_FM;

  AutomaticPresetChoose(currentFreq);

  redrawStatus = true;
  redrawScreen = true;
  newScanStart = true;

  ToggleRX(true), ToggleRX(false); // hack to prevent noise when squelch off
  SetModulation(settings.modulationType);

  RelaunchScan();

  memset(rssiHistory, 0, 128);

  isInitialized = true;

  while (isInitialized) {
    Tick();
  }
}

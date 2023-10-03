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

#define F_MIN FrequencyBandTable[0].lower
#define F_MAX FrequencyBandTable[ARRAY_SIZE(FrequencyBandTable) - 1].upper

const uint16_t RSSI_MAX_VALUE = 65535;

static uint16_t R30, R37, R3D, R43, R47, R48, R7E;
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

bool isPttPressed = false;

State currentState = SPECTRUM, previousState = SPECTRUM;

PeakInfo peak;
ScanInfo scanInfo;
KeyboardState kbd = {KEY_INVALID, KEY_INVALID, 0};

const char *bwOptions[] = {"  25k", "12.5k", "6.25k"};
const char *modulationTypeOptions[] = {" FM", " AM", "USB"};
const uint8_t modulationTypeTuneSteps[] = {100, 50, 10};
const uint8_t modTypeReg47Values[] = {1, 7, 5};

SpectrumSettings settings = {STEPS_64,
                             S_STEP_25_0kHz,
                             80000,
                             3200,
                             150,
                             true,
                             BK4819_FILTER_BW_WIDE,
                             BK4819_FILTER_BW_WIDE,
                             false,
                             -130,
                             -50};

uint32_t fMeasure = 0;
uint32_t fTx = 0;
uint32_t currentFreq, tempFreq;
uint16_t rssiHistory[128] = {};

uint8_t freqInputIndex = 0;
uint8_t freqInputDotIndex = 0;
KEY_Code_t freqInputArr[10];
char freqInputString[11] = "----------\0"; // XXXX.XXXXX\0

uint8_t menuState = 0;
uint8_t hiddenMenuState = 0;

uint16_t listenT = 0;

RegisterSpec registerSpecs[] = {
    {},
    {"LNAs", 0x13, 8, 0b11, 1},
    {"LNA", 0x13, 5, 0b111, 1},
    {"PGA", 0x13, 0, 0b111, 1},
    {"IF", 0x3D, 0, 0xFFFF, 0x2aaa},
    // {"MIX", 0x13, 3, 0b11, 1}, // TODO: hidden
};

RegisterSpec hiddenRegisterSpecs[] = {
    {},

    {"Enable Compander", 0x31, 3, 1, 1},
    {"Band-Gap Enable", 0x37, 0, 1, 1},
    {"IF", 0x3D, 0, 0xFFFF, 0x2aaa},
    {"Band Selection Thr", 0x3E, 0, 0xFFFF, 1},
    {"RF filt BW ", 0x43, 12, 0b111, 1},
    {"RF filt BW weak", 0x43, 9, 0b111, 1},
    {"BW Mode Selection", 0x43, 4, 0b11, 1},
    {"AF Output Inverse", 0x47, 13, 1, 1},
    {"AF Output Select", 0x47, 8, 0b1111, 1},
    {"AF ALC Disable", 0x4B, 5, 1, 1},
    {"AFC Range Select", 0x73, 11, 0b111, 1},
    {"AFC Disable", 0x73, 4, 1, 1},
    {"AGC Fix Mode", 0x7E, 15, 1, 1},
    {"AGC Fix Index", 0x7E, 12, 0b111, 1},

    {"LNAs 10", 0x10, 8, 0b11, 1},
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

    {"Crystal vReg Bit", 0x1A, 12, 0b1111, 1},
    {"Crystal iBit", 0x1A, 8, 0b1111, 1},
    {"PLL CP bit", 0x1F, 0, 0b1111, 1},
    {"PLL/VCO Enable", 0x30, 4, 0xF, 1},
    {"XTAL Enable", 0x37, 1, 1, 1},
    {"XTAL F Low-16bits", 0x3B, 0, 0xFFFF, 1},
    {"XTAL F High-8bits", 0x3C, 8, 0xFF, 1},
    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},

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
    {"300Hz AF Resp K Rx", 0x54, 0, 0xFFFF, 1},
    {"300Hz AF Resp K Rx", 0x55, 0, 0xFFFF, 1},
    {"3kHz AF Resp K Rx", 0x75, 0, 0xFFFF, 1},
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
    {"RF Tx Deviation", 0x40, 0, 0xFFF, 1},
    {"AFTxLPF2 filter BW", 0x43, 6, 0b111, 1},
    {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 1},
    {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 1},
    {"AFTx Filt Bypass All", 0x47, 0, 1, 1},
    {"3kHz AF Resp K Tx", 0x74, 0, 0xFFFF, 1},
    {"MIC Sensit Tuning", 0x7D, 0, 0b11111, 1},
    {"DC Filt BW Tx MIC In", 0x7E, 3, 0b111, 1},

};

uint16_t statuslineUpdateTimer = 0;

static uint8_t DBm2S(int dbm) {
  uint8_t i = 0;
  dbm *= -1;
  for (i = 0; i < ARRAY_SIZE(U8RssiMap); i++) {
    if (dbm >= U8RssiMap[i]) {
      return i;
    }
  }
  return i;
}

static int Rssi2DBm(uint16_t rssi) { return (rssi >> 1) - 160; }

uint8_t CountBits(uint16_t n) {
  uint8_t count = 0;
  while (n) {
    count++;
    n >>= 1;
  }
  return count;
}

static uint16_t GetRegMask(RegisterSpec s) {
  return (1 << CountBits(s.maxValue)) - 1;
}

static uint16_t GetRegMenuValue(RegisterSpec s) {
  return (BK4819_ReadRegister(s.num) >> s.offset) & s.maxValue;
}

static void SetRegMenuValue(RegisterSpec s, uint16_t v) {
  uint16_t reg = BK4819_ReadRegister(s.num);
  reg &= ~(GetRegMask(s) << s.offset);
  BK4819_WriteRegister(s.num, reg | (v << s.offset));
}

static void UpdateRegMenuValue(RegisterSpec s, bool add) {
  uint16_t v = GetRegMenuValue(s);

  if (add && v <= s.maxValue - s.inc) {
    v += s.inc;
  } else if (!add && v >= 0 + s.inc) {
    v -= s.inc;
  }

  SetRegMenuValue(s, v);
  redrawScreen = true;
}

// GUI functions

static void PutPixel(uint8_t x, uint8_t y, bool fill) {
  if (fill) {
    gFrameBuffer[y >> 3][x] |= 1 << (y & 7);
  } else {
    gFrameBuffer[y >> 3][x] &= ~(1 << (y & 7));
  }
}

static void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
  if (fill) {
    gStatusLine[x] |= 1 << y;
  } else {
    gStatusLine[x] &= ~(1 << y);
  }
}

static void DrawHLine(int sy, int ey, int nx, bool fill) {
  for (int i = sy; i <= ey; i++) {
    if (i < 56 && nx < 128) {
      PutPixel(nx, i, fill);
    }
  }
}

static void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                bool statusbar, bool fill) {
  uint8_t c;
  uint8_t pixels;
  const uint8_t *p = (const uint8_t *)pString;

  while ((c = *p++) && c != '\0') {
    c -= 0x20;
    for (int i = 0; i < 3; ++i) {
      pixels = gFont3x5[c][i];
      for (int j = 0; j < 6; ++j) {
        if (pixels & 1) {
          if (statusbar)
            PutPixelStatus(x + i, y + j, fill);
          else
            PutPixel(x + i, y + j, fill);
        }
        pixels >>= 1;
      }
    }
    x += 4;
  }
}

// Utility functions

KEY_Code_t GetKey() {
  KEY_Code_t btn = KEYBOARD_Poll();
  if (btn == KEY_INVALID && !GPIO_CheckBit(&GPIOC->DATA, GPIOC_PIN_PTT)) {
    btn = KEY_PTT;
  }
  return btn;
}

static int clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

static uint8_t my_abs(signed v) { return v > 0 ? v : -v; }

void SetState(State state) {
  previousState = currentState;
  currentState = state;
  redrawScreen = true;
  redrawStatus = true;
}

// Radio functions

static void ToggleAFBit(bool on) {
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  reg &= ~(1 << 8);
  if (on)
    reg |= on << 8;
  BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void BackupRegisters() {
  R30 = BK4819_ReadRegister(0x30);
  R37 = BK4819_ReadRegister(0x37);
  R3D = BK4819_ReadRegister(0x3D);
  R43 = BK4819_ReadRegister(0x43);
  R47 = BK4819_ReadRegister(0x47);
  R48 = BK4819_ReadRegister(0x48);
  R7E = BK4819_ReadRegister(0x7E);
}

static void RestoreRegisters() {
  BK4819_WriteRegister(0x30, R30);
  BK4819_WriteRegister(0x37, R37);
  BK4819_WriteRegister(0x3D, R3D);
  BK4819_WriteRegister(0x43, R43);
  BK4819_WriteRegister(0x47, R47);
  BK4819_WriteRegister(0x48, R48);
  BK4819_WriteRegister(0x7E, R7E);
}

static void SetModulation(ModulationType type) {
  RestoreRegisters();
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_47);
  reg &= ~(0b111 << 8);
  BK4819_WriteRegister(BK4819_REG_47, reg | (modTypeReg47Values[type] << 8));
  if (type == MOD_USB) {
    BK4819_WriteRegister(0x3D, 0b0010101101000101);
    BK4819_WriteRegister(BK4819_REG_37, 0x160F);
    BK4819_WriteRegister(0x48, 0b0000001110101000);
  }
}

static void ToggleAFDAC(bool on) {
  uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~(1 << 9);
  if (on)
    Reg |= (1 << 9);
  BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t f) {
  fMeasure = f;

  BK4819_SetFrequency(fMeasure);
  BK4819_PickRXFilterPathBasedOnFrequency(fMeasure);
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

static void SetTxF(uint32_t f) {
  fTx = f;
  BK4819_SetFrequency(f);
  BK4819_PickRXFilterPathBasedOnFrequency(f);
  uint16_t reg = BK4819_ReadRegister(BK4819_REG_30);
  BK4819_WriteRegister(BK4819_REG_30, 0);
  BK4819_WriteRegister(BK4819_REG_30, reg);
}

// Spectrum related

bool IsPeakOverLevel() { return peak.rssi >= settings.rssiTriggerLevel; }

static void ResetPeak() {
  peak.t = 0;
  peak.rssi = 0;
}

bool IsCenterMode() { return settings.scanStepIndex < S_STEP_2_5kHz; }
uint8_t GetStepsCount() { return 128 >> settings.stepsCount; }
uint16_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }
uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }
uint32_t GetFStart() {
  return IsCenterMode() ? currentFreq - (GetBW() >> 1) : currentFreq;
}
uint32_t GetFEnd() { return currentFreq + GetBW(); }

static void TuneToPeak() {
  scanInfo.f = peak.f;
  scanInfo.rssi = peak.rssi;
  scanInfo.i = peak.i;
  SetF(scanInfo.f);
}

static void DeInitSpectrum() {
  SetF(initialFreq);
  RestoreRegisters();
  isInitialized = false;
}

uint8_t GetBWRegValueForScan() {
  return scanStepBWRegValues[settings.scanStepIndex];
}

/* static void ResetRSSI() {
  uint32_t Reg = BK4819_ReadRegister(BK4819_REG_30);
  Reg &= ~1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
  Reg |= 1;
  BK4819_WriteRegister(BK4819_REG_30, Reg);
} */

uint16_t GetRssi() {
  // SYSTICK_DelayUs(800);
  // testing autodelay based on Glitch value
  while ((BK4819_ReadRegister(0x63) & 0b11111111) >= 255) {
    SYSTICK_DelayUs(100);
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

  return clamp(f, FrequencyBandTable[0].lower,
               FrequencyBandTable[ARRAY_SIZE(FrequencyBandTable) - 1].upper);
}

bool IsTXAllowed() { return gSetting_ALL_TX != 2; }

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
  ToggleAFDAC(on);
  ToggleAFBit(on);

  if (on) {
    listenT = 1000;
    BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
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

static void ResetBlacklist() {
  for (int i = 0; i < 128; ++i) {
    if (rssiHistory[i] == RSSI_MAX_VALUE)
      rssiHistory[i] = 0;
  }
}

static void RelaunchScan() {
  InitScan();
  ResetPeak();
  ToggleRX(false);
#ifdef SPECTRUM_AUTOMATIC_SQUELCH
  settings.rssiTriggerLevel = RSSI_MAX_VALUE;
#endif
  preventKeypress = true;
  scanInfo.rssiMin = RSSI_MAX_VALUE;
}

static void UpdateScanInfo() {
  if (scanInfo.rssi > scanInfo.rssiMax) {
    scanInfo.rssiMax = scanInfo.rssi;
    scanInfo.fPeak = scanInfo.f;
    scanInfo.iPeak = scanInfo.i;
  }

  if (scanInfo.rssi < scanInfo.rssiMin) {
    scanInfo.rssiMin = scanInfo.rssi;
    settings.dbMin = Rssi2DBm(scanInfo.rssiMin);
    redrawStatus = true;
  }
}

static void AutoTriggerLevel() {
  if (settings.rssiTriggerLevel == RSSI_MAX_VALUE) {
    settings.rssiTriggerLevel = clamp(scanInfo.rssiMax + 8, 0, RSSI_MAX_VALUE);
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

static void Measure() { rssiHistory[scanInfo.i] = scanInfo.rssi = GetRssi(); }

// Update things by keypress

static void UpdateRssiTriggerLevel(bool inc) {
  if (inc)
    settings.rssiTriggerLevel += 2;
  else
    settings.rssiTriggerLevel -= 2;
  redrawScreen = true;
  redrawStatus = true;
}

static void UpdateDBMax(bool inc) {
  if (inc && settings.dbMax < 10) {
    settings.dbMax += 1;
  } else if (!inc && settings.dbMax > settings.dbMin) {
    settings.dbMax -= 1;
  } else {
    return;
  }
  redrawStatus = true;
  redrawScreen = true;
  SYSTEM_DelayMs(20);
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
  redrawScreen = true;
}

static void UpdateFreqChangeStep(bool inc) {
  uint16_t diff = GetScanStep() * 4;
  if (inc && settings.frequencyChangeStep < 200000) {
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
  for (int i = 0; i < 10; ++i) {
    freqInputString[i] = '-';
  }
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
  for (int i = 0; i < 10; ++i) {
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
    for (int i = dotIndex + 1; i < freqInputIndex; ++i) {
      tempFreq += freqInputArr[i] * base;
      base /= 10;
    }
  }
  redrawScreen = true;
}

static void Blacklist() {
  rssiHistory[peak.i] = RSSI_MAX_VALUE;
  ResetPeak();
  ToggleRX(false);
  newScanStart = true;
}

// Draw things

int ConvertDomain(int aValue, int aMin, int aMax, int bMin, int bMax) {
  int aRange = aMax - aMin;
  int bRange = bMax - bMin;
  aValue = clamp(aValue, aMin, aMax);
  return ((aValue - aMin) * bRange + aRange / 2) / aRange + bMin;
}

// applied x2 to prevent initial rounding
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax) {
  const int DB_MIN = settings.dbMin << 1;
  const int DB_MAX = settings.dbMax << 1;

  return ConvertDomain(rssi - (160 << 1), DB_MIN, DB_MAX, pxMin, pxMax);
}

uint8_t Rssi2Y(uint16_t rssi) {
  return DrawingEndY - Rssi2PX(rssi, 0, DrawingEndY);
}

static void DrawSpectrum() {
  for (uint8_t x = 0; x < 128; ++x) {
    uint16_t rssi = rssiHistory[x >> settings.stepsCount];
    if (rssi != RSSI_MAX_VALUE) {
      DrawHLine(Rssi2Y(rssi), DrawingEndY, x, true);
    }
  }
}

static void UpdateBatteryInfo() {
  for (int i = 0; i < 4; i++) {
    BOARD_ADC_GetBatteryInfo(&gBatteryVoltages[i], &gBatteryCurrent);
  }

  uint16_t Voltage;
  gBatteryDisplayLevel = 0;

  Voltage = (gBatteryVoltages[0] + gBatteryVoltages[1] + gBatteryVoltages[2] +
             gBatteryVoltages[3]) /
            4;

  if (gBatteryCalibration[5] < Voltage) {
    gBatteryDisplayLevel = 6;
  } else if (gBatteryCalibration[4] < Voltage) {
    gBatteryDisplayLevel = 5;
  } else if (gBatteryCalibration[3] < Voltage) {
    gBatteryDisplayLevel = 4;
  } else if (gBatteryCalibration[2] < Voltage) {
    gBatteryDisplayLevel = 3;
  } else if (gBatteryCalibration[1] < Voltage) {
    gBatteryDisplayLevel = 2;
  } else if (gBatteryCalibration[0] < Voltage) {
    gBatteryDisplayLevel = 1;
  }
}

static void DrawStatus() {
#ifdef SPECTRUM_EXTRA_VALUES
  sprintf(String, "%d/%d P:%d T:%d", settings.dbMin, settings.dbMax,
          Rssi2DBm(peak.rssi), Rssi2DBm(settings.rssiTriggerLevel));
#else
  sprintf(String, "%d/%d", settings.dbMin, settings.dbMax);
#endif
  GUI_DisplaySmallest(String, 0, 1, true, true);

  UpdateBatteryInfo();

  gStatusLine[127] = 0b01111110;
  for (int i = 126; i >= 116; i--) {
    gStatusLine[i] = 0b01000010;
  }
  uint8_t v = gBatteryDisplayLevel;
  v <<= 1;
  for (int i = 125; i >= 116; i--) {
    if (126 - i <= v) {
      gStatusLine[i + 2] = 0b01111110;
    }
  }
  gStatusLine[117] = 0b01111110;
  gStatusLine[116] = 0b00011000;
}

static void DrawF(uint32_t f) {
  sprintf(String, "%u.%05u", f / 100000, f % 100000);

  if (currentState == STILL && isPttPressed) {
    if (gBatteryDisplayLevel == 6) {
      sprintf(String, "VOLTAGE HIGH");
    } else if (!IsTXAllowed()) {
      sprintf(String, "DISABLED");
    } else {
      f = GetOffsetedF(f);
      sprintf(String, "TX %u.%05u", f / 100000, f % 100000);
    }
  }
  UI_PrintStringSmall(String, 8, 127, 0);

  sprintf(String, "%s", modulationTypeOptions[settings.modulationType]);
  GUI_DisplaySmallest(String, 116, 1, false, true);
  sprintf(String, "%s", bwOptions[settings.listenBw]);
  GUI_DisplaySmallest(String, 108, 7, false, true);
}

static void DrawNums() {

  if (currentState == SPECTRUM) {
    sprintf(String, "%ux", GetStepsCount());
    GUI_DisplaySmallest(String, 0, 1, false, true);
    sprintf(String, "%u.%02uk", GetScanStep() / 100, GetScanStep() % 100);
    GUI_DisplaySmallest(String, 0, 7, false, true);
  }

  if (IsCenterMode()) {
    sprintf(String, "%u.%05u \xB1%u.%02uk", currentFreq / 100000,
            currentFreq % 100000, settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    GUI_DisplaySmallest(String, 36, 49, false, true);
  } else {
    sprintf(String, "%u.%05u", GetFStart() / 100000, GetFStart() % 100000);
    GUI_DisplaySmallest(String, 0, 49, false, true);

    sprintf(String, "\xB1%u.%02uk", settings.frequencyChangeStep / 100,
            settings.frequencyChangeStep % 100);
    GUI_DisplaySmallest(String, 48, 49, false, true);

    sprintf(String, "%u.%05u", GetFEnd() / 100000, GetFEnd() % 100000);
    GUI_DisplaySmallest(String, 93, 49, false, true);
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
  if (IsCenterMode()) {
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
  }
}

static void DrawArrow(uint8_t x) {
  for (signed i = -2; i <= 2; ++i) {
    signed v = x + i;
    if (!(v & 128)) {
      gFrameBuffer[5][v] |= (0b01111000 << my_abs(i)) & 0b01111000;
    }
  }
}

static void OnKeyDown(uint8_t key) {
  switch (key) {
  case KEY_3:
    UpdateDBMax(true);
    break;
  case KEY_9:
    UpdateDBMax(false);
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
    break;
  case KEY_MENU:
    break;
  case KEY_EXIT:
    if (menuState) {
      menuState = 0;
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
}

void OnKeyDownStill(KEY_Code_t key) {
  switch (key) {
  case KEY_3:
    UpdateDBMax(true);
    break;
  case KEY_9:
    UpdateDBMax(false);
    break;
  case KEY_UP:
    if (menuState) {
      UpdateRegMenuValue(registerSpecs[menuState], true);
      break;
    }
    if (hiddenMenuState) {
      UpdateRegMenuValue(hiddenRegisterSpecs[hiddenMenuState], true);
      break;
    }
    UpdateCurrentFreqStill(true);
    break;
  case KEY_DOWN:
    if (menuState) {
      UpdateRegMenuValue(registerSpecs[menuState], false);
      break;
    }
    if (hiddenMenuState) {
      UpdateRegMenuValue(hiddenRegisterSpecs[hiddenMenuState], false);
      break;
    }
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
    isPttPressed = true;
    if (gBatteryDisplayLevel != 6 && IsTXAllowed()) {
      ToggleTX(true);
    }
    redrawScreen = true;
    break;
  case KEY_MENU:
    hiddenMenuState = 0;
    if (menuState == ARRAY_SIZE(registerSpecs) - 1) {
      menuState = 1;
    } else {
      menuState++;
    }
    redrawScreen = true;
    break;
  case KEY_2:
    menuState = 0;
    if (hiddenMenuState <= 1) {
      hiddenMenuState = ARRAY_SIZE(hiddenRegisterSpecs) - 1;
    } else {
      hiddenMenuState--;
    }
    SYSTEM_DelayMs(90);
    break;
  case KEY_8:
    menuState = 0;
    if (hiddenMenuState == ARRAY_SIZE(hiddenRegisterSpecs) - 1) {
      hiddenMenuState = 1;
    } else {
      hiddenMenuState++;
    }
    SYSTEM_DelayMs(90);
    break;
  case KEY_EXIT:
    if (menuState) {
      menuState = 0;
      break;
    }
    if (hiddenMenuState) {
      hiddenMenuState = 0;
      break;
    }
    SetState(SPECTRUM);
    monitorMode = false;
    RelaunchScan();
    break;
  default:
    break;
  }
}

static void OnKeysReleased() {
  isPttPressed = false;
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

  for (int i = 0; i < 121; i++) {
    if (i % 10 == 0) {
      gFrameBuffer[2][i + METER_PAD_LEFT] = 0b01110000;
    } else if (i % 5 == 0) {
      gFrameBuffer[2][i + METER_PAD_LEFT] = 0b00110000;
    } else {
      gFrameBuffer[2][i + METER_PAD_LEFT] = 0b00010000;
    }
  }

  uint8_t x = Rssi2PX(scanInfo.rssi, 0, 121);
  for (int i = 0; i < x; ++i) {
    if (i % 5) {
      gFrameBuffer[2][i + METER_PAD_LEFT] |= 0b00000111;
    }
  }

  int dbm = Rssi2DBm(scanInfo.rssi);
  uint8_t s = DBm2S(dbm);
  sprintf(String, "S: %u", s);
  GUI_DisplaySmallest(String, 4, 9, false, true);
  sprintf(String, "%d dBm", dbm);
  GUI_DisplaySmallest(String, 32, 9, false, true);

  if (isTransmitting) {
    uint8_t afDB = BK4819_ReadRegister(0x6F) & 0b1111111;
    uint8_t afPX = ConvertDomain(afDB, 0, 120, 0, 121);
    for (int i = 0; i < afPX; ++i) {
      gFrameBuffer[3][i + METER_PAD_LEFT] |= 0b00000011;
    }
  }

  if (!monitorMode) {
    uint8_t x = Rssi2PX(settings.rssiTriggerLevel, 0, 121);
    gFrameBuffer[2][METER_PAD_LEFT + x] = 0b11111111;
  }

  if (!hiddenMenuState) {
    const uint8_t PAD_LEFT = 4;
    const uint8_t CELL_WIDTH = 30;
    uint8_t offset = PAD_LEFT;
    uint8_t row = 4;

    for (int i = 0, idx = 1; idx <= 4; ++i, ++idx) {
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
      GUI_DisplaySmallest(String, offset + 2, row * 8 + 2, false,
                          menuState != idx);
      sprintf(String, "%u", GetRegMenuValue(s));
      GUI_DisplaySmallest(String, offset + 2, (row + 1) * 8 + 1, false,
                          menuState != idx);
    }
  } else {
    uint8_t hiddenMenuLen = ARRAY_SIZE(hiddenRegisterSpecs);
    uint8_t offset = clamp(hiddenMenuState - 2, 1, hiddenMenuLen - 5);
    for (int i = 0; i < 5; ++i) {
      RegisterSpec s = hiddenRegisterSpecs[i + offset];
      bool isCurrent = hiddenMenuState == i + offset;
      sprintf(String, "%s%x %s: %u", isCurrent ? ">" : " ", s.num, s.name,
              GetRegMenuValue(s));
      GUI_DisplaySmallest(String, 0, i * 6 + 26, false, true);
    }
  }
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

  if (kbd.current == kbd.prev && kbd.counter <= 16) {
    kbd.counter++;
    SYSTEM_DelayMs(20);
  }

  if (kbd.counter == 3 || kbd.counter > 16) {
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
  if (rssiHistory[scanInfo.i] != RSSI_MAX_VALUE) {
    SetF(scanInfo.f);
    Measure();
    UpdateScanInfo();
  }
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

static void UpdateStill() {
  Measure();
  redrawScreen = true;
  preventKeypress = false;

  peak.rssi = scanInfo.rssi;
  AutoTriggerLevel();

  ToggleRX(IsPeakOverLevel() || monitorMode);
}

static void UpdateListening() {
  preventKeypress = false;
  if (!isListening) {
    ToggleRX(true);
  }
  if (currentState == STILL) {
    listenT = 0;
  }
  if (listenT) {
    listenT--;
    SYSTEM_DelayMs(1);
    return;
  }

  if (currentState == SPECTRUM) {
    BK4819_WriteRegister(0x43, GetBWRegValueForScan());
    Measure();
    BK4819_WriteRegister(0x43, listenBWRegValues[settings.listenBw]);
  } else {
    Measure();
  }

  peak.rssi = scanInfo.rssi;
  redrawScreen = true;

  if (IsPeakOverLevel() || monitorMode) {
    listenT = 1000;
    return;
  }

  ToggleRX(false);
  newScanStart = true;
}

static void UpdateTransmitting() {}

static void Tick() {
  if (!preventKeypress) {
    HandleUserInput();
  }
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
  if (redrawStatus || ++statuslineUpdateTimer > 4096) {
    RenderStatus();
    redrawStatus = false;
    statuslineUpdateTimer = 0;
  }
  if (redrawScreen) {
    Render();
    redrawScreen = false;
  }
}

void APP_RunSpectrum() {
  // TX here coz it always? set to active VFO
  initialFreq = gEeprom.VfoInfo[gEeprom.TX_CHANNEL].pRX->Frequency;
  currentFreq = initialFreq;

  BackupRegisters();

  redrawStatus = true;
  redrawScreen = false; // we will wait until scan done
  newScanStart = true;

  ToggleRX(true), ToggleRX(false); // hack to prevent noise when squelch off
  SetModulation(settings.modulationType = MOD_FM);
  BK4819_SetFilterBandwidth(settings.listenBw = BK4819_FILTER_BW_WIDE);

  RelaunchScan();

  for (int i = 0; i < 128; ++i) {
    rssiHistory[i] = 0;
  }

  isInitialized = true;

  while (isInitialized) {
    Tick();
  }
}

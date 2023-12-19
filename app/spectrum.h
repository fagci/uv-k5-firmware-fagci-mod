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

#ifndef SPECTRUM_H
#define SPECTRUM_H

#include "../am_fix.h"
#include "../app/finput.h"
#include "../app/uart.h"
#include "../bitmaps.h"
#include "../board.h"
#include "../bsp/dp32g030/gpio.h"
#include "../driver/bk4819-regs.h"
#include "../driver/bk4819.h"
#include "../driver/gpio.h"
#include "../driver/keyboard.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../driver/systick.h"
#include "../external/CMSIS_5/Device/ARM/ARMCM0/Include/ARMCM0.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../frequencies.h"
#include "../helper/battery.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../radio.h"
#include "../settings.h"
#include "../ui/battery.h"
#include "../ui/helper.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const uint8_t DrawingEndY = 40;

static const uint8_t gStepSettingToIndex[] = {
    [STEP_2_5kHz] = 4,  [STEP_5_0kHz] = 5,  [STEP_6_25kHz] = 6,
    [STEP_10_0kHz] = 8, [STEP_12_5kHz] = 9, [STEP_25_0kHz] = 10,
    [STEP_8_33kHz] = 7,
};

typedef enum State {
  SPECTRUM,
  FREQ_INPUT,
  STILL,
} State;

typedef enum StepsCount {
  STEPS_128,
  STEPS_64,
  STEPS_32,
  STEPS_16,
} StepsCount;

typedef STEP_Setting_t ScanStep;

typedef struct SpectrumSettings {
  StepsCount stepsCount;
  ScanStep scanStepIndex;
  uint32_t frequencyChangeStep;
  uint16_t rssiTriggerLevel;

  bool backlightState;
  BK4819_FilterBandwidth_t listenBw;
  ModulationType modulationType;
  uint16_t delayUS;
} SpectrumSettings;

typedef struct KeyboardState {
  KEY_Code_t current;
  KEY_Code_t prev;
  uint8_t counter;
} KeyboardState;

typedef struct ScanInfo {
  uint16_t rssi, rssiMin, rssiMax;
  uint8_t i, iPeak;
  uint32_t f, fPeak;
  uint16_t scanStep;
  uint8_t measurementsCount;
  bool gotRssi;
} ScanInfo;

typedef struct PeakInfo {
  uint16_t t;
  uint16_t rssi;
  uint8_t i;
  uint32_t f;
} PeakInfo;

typedef struct MovingAverage {
  uint16_t mean[128];
  uint16_t buf[4][128];
  uint16_t min, mid, max;
  uint16_t t;
} MovingAverage;

typedef struct FreqPreset {
  char name[16];
  uint32_t fStart;
  uint32_t fEnd;
  StepsCount stepsCountIndex;
  uint8_t stepSizeIndex;
  ModulationType modulationType;
  BK4819_FilterBandwidth_t listenBW;
} FreqPreset;

static const FreqPreset freqPresets[] = {
    {"160m Ham Band", 181000, 200000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"80m Ham Band", 350000, 380000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"40m Ham Band", 700000, 720000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"30m Ham Band", 1010000, 1015000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"20m Ham Band", 1400000, 1435000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"16m Broadcast", 1748000, 1790000, STEPS_128, STEP_5_0kHz, MOD_AM,
     BK4819_FILTER_BW_NARROW},
    {"17m Ham Band", 1806800, 1816800, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"15m Broadcast", 1890000, 1902000, STEPS_128, STEP_5_0kHz, MOD_AM,
     BK4819_FILTER_BW_NARROW},
    {"15m Ham Band", 2100000, 2144990, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"13m Broadcast", 2145000, 2185000, STEPS_128, STEP_5_0kHz, MOD_AM,
     BK4819_FILTER_BW_NARROW},
    {"12m Ham Band", 2489000, 2499000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"11m Broadcast", 2567000, 2610000, STEPS_128, STEP_5_0kHz, MOD_AM,
     BK4819_FILTER_BW_NARROW},
    {"CB", 2697500, 2799990, STEPS_128, STEP_5_0kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"10m Ham Band", 2800000, 2970000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"6m Ham Band", 5000000, 5400000, STEPS_128, STEP_1_0kHz, MOD_USB,
     BK4819_FILTER_BW_NARROWER},
    {"Air Band Voice", 11800000, 13500000, STEPS_128, STEP_100_0kHz, MOD_AM,
     BK4819_FILTER_BW_NARROW},
    {"2m Ham Band", 14400000, 14800000, STEPS_128, STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"Railway", 15175000, 15599990, STEPS_128, STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"Sea", 15600000, 16327500, STEPS_128, STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"Satcom", 24300000, 27000000, STEPS_128, STEP_5_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"River1", 30001250, 30051250, STEPS_64, STEP_12_5kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"River2", 33601250, 33651250, STEPS_64, STEP_12_5kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"LPD", 43307500, 43477500, STEPS_128, STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"PMR", 44600625, 44620000, STEPS_32, STEP_6_25kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"FRS/GMRS 462", 46256250, 46272500, STEPS_16, STEP_12_5kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"FRS/GMRS 467", 46756250, 46771250, STEPS_16, STEP_12_5kHz, MOD_FM,
     BK4819_FILTER_BW_NARROW},
    {"LoRa WAN", 86400000, 86900000, STEPS_128, STEP_100_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"GSM900 UP", 89000000, 91500000, STEPS_128, STEP_100_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"GSM900 DOWN", 93500000, 96000000, STEPS_128, STEP_100_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
    {"23cm Ham Band", 124000000, 130000000, STEPS_128, STEP_25_0kHz, MOD_FM,
     BK4819_FILTER_BW_WIDE},
};

static const RegisterSpec hiddenRegisterSpecs[] = {
    {},
    /* {"tail", 0x0c, 12, 0b11, 0},
    {"cdcss", 0x0c, 14, 0b11, 0},
    {"ctcss F", 0x68, 0, 0b1111111111111, 0},

    {"FSK Tx Finished INT", 0x3F, 15, 1, 1},
    {"FSK FIFO Alm Empty INT", 0x3F, 14, 1, 1},
    {"FSK Rx Finished INT", 0x3F, 13, 1, 1},
    {"FSK FIFO Alm Full INT", 0x3F, 12, 1, 1},
    {"DTMF/5TON Found INT", 0x3F, 11, 1, 1},
    {"CT/CD T Found INT", 0x3F, 10, 1, 1},
    {"CDCSS Found INT", 0x3F, 9, 1, 1},
    {"CDCSS Lost INT", 0x3F, 8, 1, 1},
    {"CTCSS Found INT", 0x3F, 7, 1, 1},
    {"CTCSS Lost INT", 0x3F, 6, 1, 1},
    {"VoX Found INT", 0x3F, 5, 1, 1},
    {"VoX Lost INT", 0x3F, 4, 1, 1},
    {"Squelch Found INT", 0x3F, 3, 1, 1},
    {"Squelch Lost INT", 0x3F, 2, 1, 1},
    {"FSK Rx Sync INT", 0x3F, 1, 1, 1}, */

    {"XTAL F Mode Select", 0x3C, 6, 0b11, 1},
    {"IF step100x", 0x3D, 0, 0xFFFF, 100},
    {"IF step1x", 0x3D, 0, 0xFFFF, 1},
    {"RFfiltBW1.7-4.5khz ", 0x43, 12, 0b111, 1},
    {"RFfiltBWweak1.7-4.5khz", 0x43, 9, 0b111, 1},
    {"AFTxLPF2fltBW1.7-4.5khz", 0x43, 6, 0b111, 1},
    {"BW Mode Selection", 0x43, 4, 0b11, 1},
    {"XTAL F Low-16bits", 0x3B, 0, 0xFFFF, 1},
    {"XTAL F Low-16bits 100", 0x3B, 0, 0xFFFF, 100},
    {"XTAL F High-8bits", 0x3C, 8, 0xFF, 1},
    {"XTAL F reserved flt", 0x3C, 0, 0b111111, 1},
    {"XTAL Enable", 0x37, 1, 1, 1},

    // {"DSP Voltage Setting", 0x37, 12, 0b111, 1},
    {"ANA LDO Selection", 0x37, 11, 1, 1},
    {"VCO LDO Selection", 0x37, 10, 1, 1},
    {"RF LDO Selection", 0x37, 9, 1, 1},
    {"PLL LDO Selection", 0x37, 8, 1, 1},
    {"ANA LDO Bypass", 0x37, 7, 1, 1},
    {"VCO LDO Bypass", 0x37, 6, 1, 1},
    {"RF LDO Bypass", 0x37, 5, 1, 1},
    {"PLL LDO Bypass", 0x37, 4, 1, 1},

    {"Freq Scan Indicator", 0x0D, 15, 1, 1},
    {"F Scan High 16 bits", 0x0D, 0, 0xFFFF, 1},
    {"F Scan Low 16 bits", 0x0E, 0, 0xFFFF, 1},

    {"AGC fix", 0x7E, 15, 0b1, 1},
    {"AGC idx", 0x7E, 12, 0b111, 1},
    {"49", 0x49, 0, 0xFFFF, 100},
    {"7B", 0x7B, 0, 0xFFFF, 100},
    {"rssi_rel", 0x65, 8, 0xFF, 1},
    {"agc_rssi", 0x62, 8, 0xFF, 1},
    {"lna_peak_rssi", 0x62, 0, 0xFF, 1},
    {"rssi_sq", 0x67, 0, 0xFF, 1},
    {"weak_rssi 1", 0x0C, 7, 1, 1},
    {"ext_lna_gain set", 0x2C, 0, 0b11111, 1},
    {"snr_out", 0x61, 8, 0xFF, 1},
    {"noise sq", 0x65, 0, 0xFF, 1},
    {"glitch", 0x63, 0, 0xFF, 1},

    {"soft_mute_en 1", 0x20, 12, 1, 1},
    {"SNR Threshold SoftMut", 0x20, 0, 0b111111, 1},
    {"soft_mute_atten", 0x20, 6, 0b11, 1},
    {"soft_mute_rate", 0x20, 8, 0b11, 1},

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
    {"300Hz AF Resp K Tx", 0x44, 0, 0xFFFF, 100},
    {"300Hz AF Resp K Tx", 0x45, 0, 0xFFFF, 100},

    /*	{"REG03 en af for afout3", 0x03, 9, 1, 1},
            {"tx mute dtmf REG_50", 0x50, 15, 1, 1},
            {"tx ctcss en REG_51", 0x51, 15, 1, 1},
            {"tx dsp en REG_30", 0x30, 1, 1, 1},
            {"disc mode dis reg30", 0x30, 8, 1, 1},
            */

};

void APP_RunSpectrum(void);

#endif /* ifndef SPECTRUM_H */

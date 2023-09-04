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

const static uint32_t F_MIN = 1800000;
const static uint32_t F_MAX = 130000000;

enum State {
    SPECTRUM,
    FREQ_INPUT,
    // MENU,
} currentState = SPECTRUM;

struct PeakInfo {
    uint8_t t;
    uint8_t rssi;
    uint8_t i;
    uint32_t f;
} peak;

enum StepsCount {
    STEPS_128,
    STEPS_64,
    STEPS_32,
    STEPS_16,
};

enum ModulationType {
    MOD_FM,
    MOD_AM,
};

enum ScanStep {
    S_STEP_0_01kHz,
    S_STEP_0_1kHz,
    S_STEP_0_5kHz,
    S_STEP_1_0kHz,

    S_STEP_2_5kHz,
    S_STEP_5_0kHz,
    S_STEP_6_25kHz,
    S_STEP_8_33kHz,
    S_STEP_10_0kHz,
    S_STEP_12_5kHz,
    S_STEP_25_0kHz,
    S_STEP_100_0kHz,
};

uint16_t scanStepValues[] = {
    1,   10,  50,  100,

    250, 500, 625, 833, 1000, 1250, 2500, 10000,
};

char *bwOptions[] = {"25k", "12.5k", "6.25k"};

typedef struct MenuItem {
    char name[8];
    struct MenuItem *items;
    uint16_t value;
    void (*callback)();
} MenuItem;

void setMode(void *m) {}

MenuItem modeItems[] = {
    {"AM", NULL, MOD_AM, NULL},
    {"FM", NULL, MOD_AM, NULL},
};

MenuItem mainMenu[5] = {
    {
        "MODE",
        modeItems,
        0,
        setMode,
    },
    {"BW"},
    {"RF"},
    {"IF"},
    {"AF"},
};

struct SpectrumSettings {
    enum StepsCount stepsCount;
    enum ScanStep scanStepIndex;
    uint32_t frequencyChangeStep;
    uint16_t scanDelay;
    uint8_t rssiTriggerLevel;

    bool isStillMode;
    int32_t stillOffset;
    bool backlightState;
    BK4819_FilterBandwidth_t listenBw;
    enum ModulationType isAMOn;
} settings = {STEPS_64, S_STEP_25_0kHz,        80000, 800, 50, false, 0,
              true,     BK4819_FILTER_BW_WIDE, false};

static const uint8_t DrawingEndY = 42;

uint8_t rssiHistory[128] = {};
uint32_t fMeasure;

uint8_t rssiMin = 255, rssiMax = 0;
uint8_t btnCounter = 0;

KEY_Code_t btn;
uint8_t btnPrev;
uint32_t currentFreq, tempFreq;
uint8_t freqInputIndex = 0;
uint8_t freqInputArr[7] = {};
uint16_t oldAFSettings;
uint16_t oldBWSettings;

bool isInitialized;
bool resetBlacklist;

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

// Radio functions

static void RestoreOldAFSettings() {
    BK4819_WriteRegister(BK4819_REG_47, oldAFSettings);
}

static void RestoreOldBWSettings() {
    BK4819_WriteRegister(0x43, oldBWSettings);
}

static void ToggleAFBit(bool on) {
    uint16_t reg = BK4819_GetRegister(BK4819_REG_47);
    reg &= ~(1 << 8);
    if (on) reg |= on << 8;
    BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAM(bool on) {
    uint16_t reg = BK4819_GetRegister(BK4819_REG_47);
    reg &= ~(0b111 << 8);
    reg |= 0b1;
    if (on) {
        reg |= 0b111 << 8;
    }
    BK4819_WriteRegister(BK4819_REG_47, reg);
}

static void ToggleAFDAC(bool on) {
    uint32_t Reg = BK4819_GetRegister(BK4819_REG_30);
    Reg &= ~(1 << 9);
    if (on) Reg |= (1 << 9);
    BK4819_WriteRegister(BK4819_REG_30, Reg);
}

bool rxState = true;
static void ToggleRX(bool on) {
    if (rxState == on) {
        return;
    }
    rxState = on;
    BK4819_ToggleGpioOut(6, on);
    ToggleAFDAC(on);
    ToggleAFBit(on);
    if (on) {
        GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
    } else {
        GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
    }
}

static void ResetRSSI() {
    uint32_t Reg = BK4819_GetRegister(BK4819_REG_30);
    Reg &= ~1;
    BK4819_WriteRegister(BK4819_REG_30, Reg);
    Reg |= 1;
    BK4819_WriteRegister(BK4819_REG_30, Reg);
}

static void SetF(uint32_t f) {
    BK4819_PickRXFilterPathBasedOnFrequency(currentFreq);
    BK4819_SetFrequency(f);
    BK4819_WriteRegister(BK4819_REG_30, 0);
    BK4819_WriteRegister(BK4819_REG_30, 0xBFF1);
}

// Spectrum related

static void ResetRSSIHistory() {
    for (int i = 0; i < 128; ++i) {
        rssiHistory[i] = 0;
    }
}
static void ResetPeak() { peak.rssi = 0; }

bool IsCenterMode() { return settings.scanStepIndex < S_STEP_2_5kHz; }
uint8_t GetStepsCount() { return 128 >> settings.stepsCount; }
uint16_t GetScanStep() { return scanStepValues[settings.scanStepIndex]; }
uint32_t GetBW() { return GetStepsCount() * GetScanStep(); }
uint32_t GetFStart() {
    return IsCenterMode() ? currentFreq - (GetBW() >> 1) : currentFreq;
}
uint32_t GetFEnd() { return currentFreq + GetBW(); }
uint32_t GetPeakF() { return peak.f + settings.stillOffset; }

static void DeInitSpectrum() {
    SetF(currentFreq);
    RestoreOldAFSettings();
    RestoreOldBWSettings();
    BK4819_WriteRegister(0x43, oldBWSettings);
    isInitialized = false;
}

uint8_t GetBWIndex() {
    uint16_t step = GetScanStep();
    if (step < 1250) {
        return BK4819_FILTER_BW_NARROWER;
    } else if (step < 2500) {
        return BK4819_FILTER_BW_NARROW;
    } else {
        return BK4819_FILTER_BW_WIDE;
    }
}

uint8_t GetRssi() {
    ResetRSSI();
    SYSTICK_DelayUs(settings.scanDelay);
    return clamp(BK4819_GetRSSI(), 0, 255);
}

// Update things by keypress

static void UpdateRssiTriggerLevel(int diff) {
    settings.rssiTriggerLevel += diff;
}

static void UpdateScanStep(int diff) {
    if ((diff > 0 && settings.scanStepIndex < S_STEP_100_0kHz) ||
        (diff < 0 && settings.scanStepIndex > 0)) {
        settings.scanStepIndex += diff;
        BK4819_SetFilterBandwidth(GetBWIndex());
        rssiMin = 255;
        settings.frequencyChangeStep = GetBW() >> 1;
    }
}

static void UpdateCurrentFreq(long int diff) {
    if (settings.isStillMode) {
        settings.stillOffset += diff > 0 ? 50 : -50;
        peak.i = (GetPeakF() - GetFStart()) / GetScanStep();
        ResetRSSIHistory();
        return;
    }
    if ((diff > 0 && currentFreq < F_MAX) ||
        (diff < 0 && currentFreq > F_MIN)) {
        currentFreq += diff;
    }
}

static void UpdateFreqChangeStep(long int diff) {
    settings.frequencyChangeStep =
        clamp(settings.frequencyChangeStep + diff, 10000, 200000);
}

static void Blacklist() { rssiHistory[peak.i] = 255; }

// Draw things

uint8_t Rssi2Y(uint8_t rssi) {
    return DrawingEndY - clamp(rssi - rssiMin, 0, DrawingEndY);
}

static void DrawSpectrum() {
    for (uint8_t x = 0; x < 128; ++x) {
        uint8_t v = rssiHistory[x >> settings.stepsCount];
        if (v != 255) {
            DrawHLine(Rssi2Y(v), DrawingEndY, x, true);
        }
    }
}

static void DrawStatus() {
    char String[32];

    if (settings.isStillMode) {
        sprintf(String, "Offset: %2.1fkHz %s %s", settings.stillOffset * 1e-2,
                settings.isAMOn ? "AM" : "FM", bwOptions[settings.listenBw]);
        GUI_DisplaySmallest(String, 1, 2, true, true);
    } else {
        sprintf(String, "%dx%3.2fk %1.1fms %s %s", GetStepsCount(),
                GetScanStep() * 1e-2, settings.scanDelay * 1e-3,
                settings.isAMOn ? "AM" : "FM", bwOptions[settings.listenBw]);
        GUI_DisplaySmallest(String, 1, 2, true, true);
    }
}

static void DrawNums() {
    char String[16];

    sprintf(String, "%3.3f", GetPeakF() * 1e-5);
    UI_PrintString(String, 2, 127, 0, 8, 1);

    if (IsCenterMode()) {
        sprintf(String, "%04.5f \xB1%1.2fk", currentFreq * 1e-5,
                settings.frequencyChangeStep * 1e-2);
        GUI_DisplaySmallest(String, 36, 49, false, true);
    } else {
        sprintf(String, "%04.4f", GetFStart() * 1e-5);
        GUI_DisplaySmallest(String, 0, 49, false, true);

        sprintf(String, "\xB1%1.0fk", settings.frequencyChangeStep * 1e-2);
        GUI_DisplaySmallest(String, 56, 49, false, true);

        sprintf(String, "%04.4f", GetFEnd() * 1e-5);
        GUI_DisplaySmallest(String, 96, 49, false, true);
    }
}

static void DrawRssiTriggerLevel() {
    uint8_t y = Rssi2Y(settings.rssiTriggerLevel);
    for (uint8_t x = 0; x < 128; x += 2) {
        PutPixel(x, y, true);
    }
}

static void DrawTicks() {
    uint32_t f = GetFStart() % 100000;
    uint32_t step = GetScanStep();
    for (uint8_t i = 0; i < 128; i += (1 << settings.stepsCount), f += step) {
        uint8_t barValue = 0b00000100;
        (f % 10000) < step && (barValue |= 0b00001000);
        (f % 50000) < step && (barValue |= 0b00010000);
        (f % 100000) < step && (barValue |= 0b01100000);

        gFrameBuffer[5][i] |= barValue;
    }

    // center
    gFrameBuffer[5][64] = 0b10101000;
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
        case KEY_1:
            if (settings.scanDelay < 8000) {
                settings.scanDelay += 100;
                rssiMin = 255;
            }
            break;
        case KEY_7:
            if (settings.scanDelay > 400) {
                settings.scanDelay -= 100;
                rssiMin = 255;
            }
            break;
        case KEY_3:
            UpdateScanStep(1);
            resetBlacklist = true;
            break;
        case KEY_9:
            UpdateScanStep(-1);
            resetBlacklist = true;
            break;
        case KEY_2:
            UpdateFreqChangeStep(GetScanStep() * 4);
            break;
        case KEY_8:
            UpdateFreqChangeStep(-GetScanStep() * 4);
            break;
        case KEY_UP:
            UpdateCurrentFreq(settings.frequencyChangeStep);
            resetBlacklist = true;
            break;
        case KEY_DOWN:
            UpdateCurrentFreq(-settings.frequencyChangeStep);
            resetBlacklist = true;
            break;
        case KEY_0:
            Blacklist();
            break;
        case KEY_STAR:
            UpdateRssiTriggerLevel(1);
            SYSTEM_DelayMs(90);
            break;
        case KEY_F:
            UpdateRssiTriggerLevel(-1);
            SYSTEM_DelayMs(90);
            break;
        case KEY_MENU:
            currentState = FREQ_INPUT;
            freqInputIndex = 0;
            break;
        case KEY_4:
            settings.isAMOn = !settings.isAMOn;
            ToggleAM(settings.isAMOn);
            break;
        case KEY_SIDE1:
            if (settings.stepsCount == STEPS_128) {
                settings.stepsCount = STEPS_16;
                break;
            }
            settings.stepsCount--;
            break;
        /* case KEY_SIDE2:
            currentState = MENU;
            break; */
        case KEY_SIDE2:
          if (settings.listenBw == BK4819_FILTER_BW_NARROWER) {
            settings.listenBw = BK4819_FILTER_BW_WIDE;
            break;
          }
          settings.listenBw++;
          break;
        case KEY_5:
            settings.backlightState = !settings.backlightState;
            if (settings.backlightState) {
                GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
            } else {
                GPIO_ClearBit(&GPIOB->DATA, GPIOB_PIN_BACKLIGHT);
            }
            break;
        case KEY_6:
            settings.isStillMode = !settings.isStillMode;
            if (settings.isStillMode) {
                ResetRSSIHistory();
            } else {
                settings.stillOffset = 0;
            }
            break;
        case KEY_EXIT:
            DeInitSpectrum();
            break;
        case KEY_PTT:
            if (settings.isStillMode) {
                // TODO: tx
            }
    }
    ResetPeak();
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
            if (freqInputIndex < 7) {
                freqInputArr[freqInputIndex++] = key;
            }
            break;
        case KEY_EXIT:
            if (freqInputIndex == 0) {
                currentState = SPECTRUM;
                break;
            }
            freqInputIndex--;
            break;
        case KEY_MENU:
            if (tempFreq >= F_MIN && tempFreq <= F_MAX) {
                peak.f = currentFreq = tempFreq;
                settings.stillOffset = 0;
                resetBlacklist = true;
                currentState = SPECTRUM;
                peak.i = GetStepsCount() >> 1;
                ResetRSSIHistory();
            }
            break;
    }
}

/* uint8_t menuItemIndex = 0;
const uint8_t MENU_SIZE = sizeof(mainMenu) / sizeof(MenuItem);
void OnMenuInput(KEY_Code_t btn) {
    switch (btn) {
        case KEY_DOWN:
            if (menuItemIndex < MENU_SIZE - 1) {
                menuItemIndex++;
                break;
            }
            menuItemIndex = 0;
            break;
        case KEY_UP:
            if (menuItemIndex > 0) {
                menuItemIndex--;
                break;
            }
            menuItemIndex = MENU_SIZE - 1;
            break;
    }
} */

static void RenderFreqInput() {
    tempFreq = 0;

    for (int i = 0; i < freqInputIndex; ++i) {
        tempFreq *= 10;
        tempFreq += freqInputArr[i];
    }
    tempFreq *= 100;

    char String[16];

    sprintf(String, "%4.3f", tempFreq * 1e-5);
    UI_PrintString(String, 2, 127, 0, 8, 1);
}

/* void RenderMenu() {
    char String[32];
    int x = 0;
    uint8_t itemSize;
    bool selected = false;
    for (int i = 0; i < MENU_SIZE; ++i) {
        itemSize = (strlen(mainMenu[i].name) << 2) + 3;
        selected = i == menuItemIndex;
        sprintf(String, mainMenu[i].name);
        if (selected) {
            for (int j = 0; j < itemSize; ++j) {
                gStatusLine[x + j] = 0b01111111;
            }
        }
        GUI_DisplaySmallest(String, x + 2, 1, true, !selected);
        x += itemSize + 2;
    }
} */

static void RenderStatus() {
    memset(gStatusLine, 0, sizeof(gStatusLine));
    DrawStatus();
    ST7565_BlitStatusLine();
}

static void Render() {
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
    if (currentState == SPECTRUM /*|| currentState == MENU*/) {
        DrawTicks();
        DrawArrow(peak.i << settings.stepsCount);
        if (rssiMin != 255) {
            DrawSpectrum();
        }
        DrawRssiTriggerLevel();
        DrawNums();
    }

    /* if (currentState == MENU) {
        RenderMenu();
    } */

    if (currentState == FREQ_INPUT) {
        RenderFreqInput();
    }

    ST7565_BlitFullScreen();
}

bool HandleUserInput() {
    btnPrev = btn;
    btn = GetKey();

    if (btn == 255) {
        btnCounter = 0;

        return true;
    }

    if (btn == btnPrev && btnCounter < 255) {
        btnCounter++;
        SYSTEM_DelayMs(20);
    }
    if (btnPrev == 255 || btnCounter > 16) {
        switch (currentState) {
            case SPECTRUM:
                OnKeyDown(btn);
                break;
            case FREQ_INPUT:
                OnKeyDownFreqInput(btn);
                break;
            /* case MENU:
                OnMenuInput(btn);
                break; */
        }
        RenderStatus();
    }

    return true;
}

static void Scan() {
    uint8_t rssi = 0;
    uint8_t iPeak = 0;
    uint32_t fPeak = currentFreq;

    rssiMax = 0;
    fMeasure = GetFStart();

    uint16_t scanStep = GetScanStep();
    uint8_t measurementsCount = GetStepsCount();

    for (uint8_t i = 0;
         i < measurementsCount && (GetKey() == 255 || resetBlacklist);
         ++i, fMeasure += scanStep) {
        if (!resetBlacklist && rssiHistory[i] == 255) {
            continue;
        }
        SetF(fMeasure);
        rssi = rssiHistory[i] = GetRssi();
        if (rssi > rssiMax) {
            rssiMax = rssi;
            fPeak = fMeasure;
            iPeak = i;
        }
        if (rssi < rssiMin) {
            rssiMin = rssi;
        }
    }
    resetBlacklist = false;
    ++peak.t;

    if (rssiMax > peak.rssi || peak.t >= 16) {
        peak.t = 0;
        peak.rssi = rssiMax;
        peak.f = fPeak;
        peak.i = iPeak;
    }
}

static void Listen() {
    if (fMeasure != GetPeakF()) {
        fMeasure = GetPeakF();
        SetF(fMeasure);
    }
    ToggleRX(peak.rssi >= settings.rssiTriggerLevel);
    BK4819_SetFilterBandwidth(settings.listenBw);
    for (uint8_t i = 0; i < 50 && GetKey() == 255; ++i) {
        SYSTEM_DelayMs(20);
    }
    BK4819_SetFilterBandwidth(GetBWIndex());
    peak.rssi = rssiHistory[peak.i] = GetRssi();
}

static void Update() {
    if (settings.isStillMode || peak.rssi >= settings.rssiTriggerLevel) {
        Listen();
    }
    if (rssiMin == 255 ||
        (!settings.isStillMode && peak.rssi < settings.rssiTriggerLevel)) {
        ToggleRX(false);
        BK4819_SetFilterBandwidth(GetBWIndex());
        Scan();
    }
}

static void Tick() {
    if (HandleUserInput()) {
        switch (currentState) {
            // case MENU:
            case SPECTRUM:
                Update();
                break;
            case FREQ_INPUT:
                break;
        }
        Render();
    }
}

void APP_RunSpectrum() {
    currentFreq = BK4819_GetFrequency();
    oldAFSettings = BK4819_GetRegister(0x47);
    oldBWSettings = BK4819_GetRegister(0x43);
    BK4819_SetFilterBandwidth(GetBWIndex());
    ResetPeak();
    resetBlacklist = true;
    ToggleRX(false);
    isInitialized = true;
    RenderStatus();
    while (isInitialized) {
        Tick();
    }
}

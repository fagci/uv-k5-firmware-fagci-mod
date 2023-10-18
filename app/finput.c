#include "finput.h"
#include <string.h>

KEY_Code_t freqInputArr[10];
uint8_t freqInputDotIndex = 0;

const uint8_t FREQ_INPUT_LENGTH = 10;
char freqInputString[] = "----------"; // XXXX.XXXXX
uint8_t freqInputIndex = 0;
uint32_t tempFreq;

void ResetFreqInput() {
  tempFreq = 0;
  memset(freqInputString, '-', 10);
}

void FreqInput() {
  freqInputIndex = 0;
  freqInputDotIndex = 0;
  ResetFreqInput();
}

void UpdateFreqInput(KEY_Code_t key) {
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
}

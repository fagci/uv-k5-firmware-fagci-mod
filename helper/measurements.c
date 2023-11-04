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

#include "../helper/measurements.h"

int Clamp(int v, int min, int max) {
  return v <= min ? min : (v >= max ? max : v);
}

int ConvertDomain(int aValue, int aMin, int aMax, int bMin, int bMax) {
  const int aRange = aMax - aMin;
  const int bRange = bMax - bMin;
  aValue = Clamp(aValue, aMin, aMax);
  return ((aValue - aMin) * bRange + aRange / 2) / aRange + bMin;
}

uint8_t DBm2S(int dbm) {
  uint8_t i = 0;
  dbm *= -1;
  for (i = 0; i < ARRAY_SIZE(rssi2sVHF); i++) {
    if (dbm >= rssi2sVHF[i]) {
      return i;
    }
  }
  return i;
}

int Rssi2DBm(uint16_t rssi) { return (rssi >> 1) - 160; }

// applied x2 to prevent initial rounding
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax) {
  return ConvertDomain(rssi - 320, -260, -120, pxMin, pxMax);
}

int Mid(uint16_t *array, uint8_t n) {
  int32_t sum = 0;
  for (int i = 0; i < n; ++i) {
    sum += array[i];
  }
  return sum / n;
}

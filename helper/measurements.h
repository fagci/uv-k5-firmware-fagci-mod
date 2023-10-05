#ifndef MEASUREMENTS_H
#define MEASUREMENTS_H

#include "../misc.h"
#include <stdint.h>

static const uint8_t U8RssiMap[] = {
    141, 135, 129, 123, 117, 111, 105, 99, 93, 83, 73, 63,
};

int Clamp(int v, int min, int max);
int ConvertDomain(int aValue, int aMin, int aMax, int bMin, int bMax);
uint8_t Rssi2PX(uint16_t rssi, uint8_t pxMin, uint8_t pxMax);
uint8_t DBm2S(int dbm);
int Rssi2DBm(uint16_t rssi);

#endif /* end of include guard: MEASUREMENTS_H */

// vim: ft=c

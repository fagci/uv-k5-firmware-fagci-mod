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

#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>

enum BEEP_Type_t {
  BEEP_NONE = 0U,
  BEEP_1KHZ_60MS_OPTIONAL = 1U,
  BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL = 2U,
  BEEP_440HZ_500MS = 3U,
  BEEP_500HZ_60MS_DOUBLE_BEEP = 4U,
  BEEP_TEST = 5U,
};

typedef enum BEEP_Type_t BEEP_Type_t;
extern BEEP_Type_t gBeepToPlay;

typedef struct Note {
  uint16_t f;
  uint16_t dur;
} Note;

static const Note MELODY_NOKIA[] = {
    {1319, 133}, {1175, 133}, {740, 267}, {831, 267}, {1109, 133},
    {988, 133},  {587, 267},  {659, 267}, {988, 133}, {880, 133},
    {554, 267},  {659, 267},  {880, 533},
};

static const Note MELODY_TETRIS[] = {
    {1319, 375}, {988, 188},  {1047, 188}, {1175, 188}, {1319, 94},
    {1175, 94},  {1047, 188}, {988, 188},  {880, 375},  {880, 188},
    {1047, 188}, {1319, 375}, {1175, 188}, {1047, 188}, {988, 375},
    {988, 188},  {1047, 188}, {1175, 375}, {1319, 375}, {1047, 375},
    {880, 375},  {880, 750},  {1175, 375}, {1397, 188}, {1760, 375},
    {1568, 188}, {1397, 188}, {1319, 375}, {1319, 188}, {1047, 188},
    {1319, 375}, {1175, 188}, {1047, 188}, {988, 375},  {988, 188},
    {1047, 188}, {1175, 375}, {1319, 375}, {1047, 375}, {880, 375},
    {880, 375},
};

static const Note MELODY_ARKANOID[] = {
    {1568, 214}, {1568, 161}, {1865, 857}, {1760, 214},
    {1568, 214}, {1397, 214}, {1760, 214}, {1568, 857},
};

void AUDIO_PlayBeep(BEEP_Type_t Beep);
void AUDIO_PlayMelody(const Note *melody, uint8_t size);

#endif

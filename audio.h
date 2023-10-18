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
};

typedef enum BEEP_Type_t BEEP_Type_t;
extern BEEP_Type_t gBeepToPlay;

void AUDIO_PlayBeep(BEEP_Type_t Beep);

#endif


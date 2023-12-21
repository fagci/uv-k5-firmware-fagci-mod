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

#include <string.h>
#if defined(ENABLE_FMRADIO)
#include "app/fm.h"
#endif
#include "scanner.h"
#include "../bsp/dp32g030/gpio.h"
#include "../driver/bk4819.h"
#include "../driver/eeprom.h"
#include "../driver/gpio.h"
#include "../driver/system.h"
#include "dtmf.h"
#include "../external/printf/printf.h"
#include "../misc.h"
#include "../settings.h"
#include "../ui/ui.h"

char gDTMF_String[15];
char gDTMF_InputBox[15];
uint8_t gDTMF_PreviousIndex;
uint8_t InputIndex;
bool InputMode;

char DTMF_GetCharacter(uint8_t Code)
{
	switch(Code) {
	case 0: case 1: case 2: case 3:
	case 4: case 5: case 6: case 7:
	case 8: case 9:
		return '0' + (char)Code;
	case 10:
		return 'A';
	case 11:
		return 'B';
	case 12:
		return 'C';
	case 13:
		return 'D';
	case 14:
		return '*';
	case 15:
		return '#';
	}

	return 0xFF;
}

void DTMF_Append(char Code)
{
	if (InputIndex == 0) {
		memset(gDTMF_InputBox, '-', sizeof(gDTMF_InputBox));
		gDTMF_InputBox[14] = 0;
	} else if (InputIndex >= sizeof(gDTMF_InputBox)) {
		return;
	}
	gDTMF_InputBox[InputIndex++] = Code;
}




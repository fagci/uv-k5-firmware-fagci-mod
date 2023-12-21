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
char gDTMF_Received[16];
bool gIsDtmfContactValid;
char gDTMF_ID[4];
char gDTMF_Caller[4];
char gDTMF_Callee[4];
DTMF_State_t gDTMF_State;
bool gDTMF_DecodeRing;
uint8_t gDTMF_DecodeRingCountdown;
uint8_t gDTMFChosenContact;
uint8_t gDTMF_WriteIndex;
uint8_t gDTMF_PreviousIndex;
uint8_t gDTMF_AUTO_RESET_TIME;
uint8_t gDTMF_InputIndex;
bool gDTMF_InputMode;
uint8_t gDTMF_RecvTimeout;
DTMF_CallState_t gDTMF_CallState;
DTMF_ReplyState_t gDTMF_ReplyState;
DTMF_CallMode_t gDTMF_CallMode;
bool gDTMF_IsTx;
uint8_t gDTMF_TxStopCountdown;
bool gDTMF_IsGroupCall;

bool DTMF_GetContact(uint8_t Index, char *pContact)
{
	EEPROM_ReadBuffer(0x1C00 + (Index * 0x10), pContact, 16);
	if (pContact[0] < ' ' || pContact[0] > 0x7E) {
		return false;
	}

	return true;
}

bool DTMF_FindContact(const char *pContact, char *pResult)
{
	char Contact [16];
	uint8_t i, j;

	for (i = 0; i < 16; i++) {
		if (!DTMF_GetContact(i, Contact)) {
			return false;
		}
		for (j = 0; j < 3; j++) {
			if (pContact[j] != Contact[j + 8]) {
				break;
			}
		}
		if (j == 3) {
			memcpy(pResult, Contact, 8);
			pResult[8] = 0;
			return true;
		}
	}

	return false;
}

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

bool DTMF_CompareMessage(const char *pMsg, const char *pTemplate, uint8_t Size, bool bCheckGroup)
{
	uint8_t i;

	for (i = 0; i < Size; i++) {
		if (pMsg[i] != pTemplate[i]) {
			if (!bCheckGroup) {
				return false;
			}
			gDTMF_IsGroupCall = true;
		}
	}

	return true;
}

bool DTMF_CheckGroupCall(const char *pMsg, uint32_t Size)
{
	uint32_t i;

	for (i = 0; i < Size; i++) {
		if (pMsg[i] == 7) {
			break;
		}
	}
	if (i != Size) {
		return true;
	}

	return false;
}

void DTMF_Append(char Code)
{
	if (gDTMF_InputIndex == 0) {
		memset(gDTMF_InputBox, '-', sizeof(gDTMF_InputBox));
		gDTMF_InputBox[14] = 0;
	} else if (gDTMF_InputIndex >= sizeof(gDTMF_InputBox)) {
		return;
	}
	gDTMF_InputBox[gDTMF_InputIndex++] = Code;
}

void DTMF_HandleRequest(void)
{

}

void DTMF_Reply(void)
{

}


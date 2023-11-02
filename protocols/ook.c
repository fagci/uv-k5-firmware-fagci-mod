/* Copyright Francesco
 * https://github.com/motorello
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

#include "ook.h"

void OOK_BeginTx(void) {
  RADIO_enableTX();
  BK4819_SetupPowerAmplifier(gCurrentVfo->TXP_CalculatedSetting,
                             gCurrentVfo->pTX->Frequency);
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true); // PA on
  BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, true); // turn the RED LED on
  GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
}

void OOK_EndTx(void) {
  RADIO_disableTX();
  BK4819_SetupPowerAmplifier(0, 0);
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false); // PA off
  BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false); // turn the RED LED off
  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
}

void OOK_HardwareTxOn(void) {
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true); // PA on
}

void OOK_HardwareTxOff(void) {
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false); // PA off
}

void OOK_EncodeSymbol(OOK_t *ook_struct, bool symbol) {
  if (symbol) {
    OOK_HardwareTxOff();
    SYSTICK_DelayUs(ook_struct->period_us - ook_struct->pulse_0_us);
    OOK_HardwareTxOn();
    SYSTICK_DelayUs(ook_struct->pulse_1_us);
  } else {
    OOK_HardwareTxOff();
    SYSTICK_DelayUs(ook_struct->period_us - ook_struct->pulse_0_us);
    OOK_HardwareTxOn();
    SYSTICK_DelayUs(ook_struct->pulse_0_us);
  }
}

void OOK_TxSequence(OOK_t *ook_struct) {
  uint8_t len = ook_struct->sequence_len;
  uint8_t symbol_pos = 0;
  uint8_t b = 0;
  uint8_t *ptr_to_ook_sequence = ook_struct->sequence_ptr;

  OOK_HardwareTxOn(); // send initial mark for sequence transmission
  SYSTICK_DelayUs(ook_struct->sync_pulse_us);

  while (len--) {
    if ((symbol_pos++ % 8) == 0) // getting new byte from sequence every 8 bits
    {
      b = *(uint8_t *)ook_struct->sequence_ptr++;
    }

    // sending msb first
    bool symbol = (b & 0x80) ? true : false;
    OOK_EncodeSymbol(ook_struct, symbol);
    b <<= 1;
  }

  // reset the sequence pointer to the beginning of sequence for the next call
  // of this function
  ook_struct->sequence_ptr = ptr_to_ook_sequence;

  // stop transmission for the trailing delay
  OOK_HardwareTxOff();
}

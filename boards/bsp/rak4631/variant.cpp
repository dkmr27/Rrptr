/*
  Copyright (c) 2014-2015 Arduino LLC.  All right reserved.
  Copyright (c) 2016 Sandeep Mistry All right reserved.
  Copyright (c) 2018, Adafruit Industries (adafruit.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "variant.h"
#include "wiring_constants.h"
#include "wiring_digital.h"
#include "nrf.h"

const uint32_t g_ADigitalPinMap[] =
{
  // P0
  0 , 1 , 2 , 3 , 4 , 5 , 6 , 7 ,
  8 , 9 , 10, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 20, 21, 22, 23,
  24, 25, 26, 27, 28, 29, 30, 31,

  // P1
  32, 33, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 46, 47
};


void initVariant()
{
  pinMode(PIN_ANT_SWITCH_EN, OUTPUT);
  digitalWrite(PIN_ANT_SWITCH_EN, HIGH);

  // Switched 3V3 rail on, and a RAK15001's /WP and /HOLD high, whichever slot it is
  // in. Otherwise it may ignore commands or refuse writes.
  static const uint8_t WB_PINS[] = { PIN_WB_IO1, PIN_WB_IO2, PIN_WB_IO4, PIN_WB_IO5, PIN_WB_IO6 };
  for (uint8_t pin : WB_PINS)
  {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  // Keep the external flash deselected until it is initialised
  pinMode(PIN_EXTERNAL_FLASH_CS, OUTPUT);
  digitalWrite(PIN_EXTERNAL_FLASH_CS, HIGH);

  pinMode(PIN_LED1, OUTPUT);
  ledOff(PIN_LED1);
  pinMode(PIN_LED2, OUTPUT);
  ledOff(PIN_LED2);
}

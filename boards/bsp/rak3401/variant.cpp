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
  // Both chips on the shared SPI bus deselected before anything uses it, so
  // neither takes the other's traffic as commands
  pinMode(PIN_EXTERNAL_FLASH_CS, OUTPUT);
  digitalWrite(PIN_EXTERNAL_FLASH_CS, HIGH);
  pinMode(PIN_RADIO_CS, OUTPUT);
  digitalWrite(PIN_RADIO_CS, HIGH);

  // Switched 3V3 rail (and a RAK13302's PA boost) on, and the RAK15001's /WP and
  // /HOLD high: it must be in slot A, where they are IO2 and IO1. The other slots'
  // IO lines are the radio's.
  static const uint8_t WB_PINS[] = { PIN_WB_IO1, PIN_WB_IO2 };
  for (uint8_t pin : WB_PINS)
  {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
  }

  pinMode(PIN_LED1, OUTPUT);
  ledOff(PIN_LED1);
  pinMode(PIN_LED2, OUTPUT);
  ledOff(PIN_LED2);
}

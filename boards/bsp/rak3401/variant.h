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

// RAK3401 WisBlock core: the RAK4631's nRF52840 without a radio of its own, for an
// IO-slot radio such as the RAK13302. Arduino core view of the board only; the
// radio and other
// Rrptr hardware specifics are in the variant's board_config.h.
//
// Pin numbers are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx); the pin
// map in variant.cpp is identity.

#ifndef _VARIANT_RAK3401_
#define _VARIANT_RAK3401_

/** Master clock frequency */
#define VARIANT_MCK       (64000000ul)

#define USE_LFXO      // Board uses 32khz crystal for LF

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "WVariant.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

// Number of pins defined in PinDescription array
#define PINS_COUNT           (48)
#define NUM_DIGITAL_PINS     (48)
#define NUM_ANALOG_INPUTS    (8)
#define NUM_ANALOG_OUTPUTS   (0)

// LEDs
#define PIN_LED1             (35)      // P1.03, green
#define PIN_LED2             (36)      // P1.04, blue
#define LED_BUILTIN          PIN_LED1
#define LED_GREEN            PIN_LED1
#define LED_BLUE             PIN_LED2
#define LED_STATE_ON         1         // State when LED is lit

// WisBlock IO pins. IO2 also enables the switched 3V3 rail on the base board. A
// RAK15001 has its /WP and /HOLD on these, which ones depending on the slot:
// A: IO2, IO1. B: IO1, IO2. C: IO4, IO4. D: IO6, IO5.
#define PIN_WB_IO1           (17)      // P0.17
#define PIN_WB_IO2           (34)      // P1.02
#define PIN_WB_IO4           (4)       // P0.04
#define PIN_WB_IO5           (9)       // P0.09 (NFC pin)
#define PIN_WB_IO6           (10)      // P0.10 (NFC pin)

/*
 * Analog pins
 */
#define PIN_A0               (5)       // P0.05 / AIN3, battery voltage
#define PIN_A1               (31)      // P0.31 / AIN7
#define PIN_A2               (28)      // P0.28 / AIN4
#define PIN_A3               (29)      // P0.29 / AIN5
#define PIN_A4               (30)      // P0.30 / AIN6
#define PIN_A5               (2)       // P0.02 / AIN0
#define PIN_A6               (3)       // P0.03 / AIN1
#define PIN_A7               (4)       // P0.04 / AIN2

static const uint8_t A0  = PIN_A0 ;
static const uint8_t A1  = PIN_A1 ;
static const uint8_t A2  = PIN_A2 ;
static const uint8_t A3  = PIN_A3 ;
static const uint8_t A4  = PIN_A4 ;
static const uint8_t A5  = PIN_A5 ;
static const uint8_t A6  = PIN_A6 ;
static const uint8_t A7  = PIN_A7 ;
#define ADC_RESOLUTION    14

#define PIN_VBAT             PIN_A0

// Other pins
#define PIN_AREF           (2)
static const uint8_t AREF = PIN_AREF;

/*
 * Serial interfaces
 */
#define PIN_SERIAL1_RX      (15)       // P0.15, RXD1 on the base board
#define PIN_SERIAL1_TX      (16)       // P0.16, TXD1 on the base board

/*
 * SPI Interfaces
 *
 * The default SPI bus (SPIM3) reaches the base board's slots. The RAK15001 flash
 * and an IO-slot radio share it, each with its own chip select.
 */
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_SCK          (3)       // P0.03
#define PIN_SPI_MOSI         (30)      // P0.30
#define PIN_SPI_MISO         (29)      // P0.29
#define PIN_EXTERNAL_FLASH_CS (26)     // P0.26
#define PIN_RADIO_CS         (28)      // P0.28 / IO7: a RAK13302 with R7 moved to R4

static const uint8_t SS   = PIN_EXTERNAL_FLASH_CS ;
static const uint8_t MOSI = PIN_SPI_MOSI ;
static const uint8_t MISO = PIN_SPI_MISO ;
static const uint8_t SCK  = PIN_SPI_SCK ;

/*
 * Wire Interfaces
 */
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (13)      // P0.13
#define PIN_WIRE_SCL         (14)      // P0.14

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#endif

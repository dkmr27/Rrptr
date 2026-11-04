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

// Seeed XIAO nRF52840 (and Sense). Arduino core view of the board only; the
// radio and other Rrptr hardware specifics are in each variant's
// board_config.h, so several variants (different radios) share this.
//
// Pin numbers are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx);
// the pin map in variant.cpp is identity. XIAO "Dn" labels are given in the
// comments for reference when wiring.

#ifndef _VARIANT_SEEED_XIAO_NRF52840_
#define _VARIANT_SEEED_XIAO_NRF52840_

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

// LEDs, active low
#define PIN_LED1             (26)      // P0.26, red
#define LED_BUILTIN          PIN_LED1
#define LED_RED              PIN_LED1
#define LED_GREEN            (30)      // P0.30
#define LED_BLUE             (6)       // P0.06
#define LED_STATE_ON         0         // State when LED is lit

// Battery: 1M / 510k divider on P0.31, its low side switched by P0.14.
// P0.14 must stay LOW: when high it can see more than its 3.6 V limit.
#define PIN_VBAT_ENABLE      (14)      // P0.14

/*
 * Analog pins
 */
#define PIN_A0               (2)       // P0.02 / D0
#define PIN_A1               (3)       // P0.03 / D1
#define PIN_A2               (28)      // P0.28 / D2
#define PIN_A3               (29)      // P0.29 / D3
#define PIN_A4               (4)       // P0.04 / D4
#define PIN_A5               (5)       // P0.05 / D5
#define PIN_A6               (30)      // P0.30 / AIN6
#define PIN_A7               (31)      // P0.31 / AIN7, battery voltage

static const uint8_t A0  = PIN_A0 ;
static const uint8_t A1  = PIN_A1 ;
static const uint8_t A2  = PIN_A2 ;
static const uint8_t A3  = PIN_A3 ;
static const uint8_t A4  = PIN_A4 ;
static const uint8_t A5  = PIN_A5 ;
static const uint8_t A6  = PIN_A6 ;
static const uint8_t A7  = PIN_A7 ;
#define ADC_RESOLUTION    14

#define PIN_VBAT             PIN_A7

// Other pins
#define PIN_AREF           (2)
static const uint8_t AREF = PIN_AREF;

/*
 * Serial interfaces
 */
#define PIN_SERIAL1_RX      (44)       // P1.12 / D7
#define PIN_SERIAL1_TX      (43)       // P1.11 / D6

/*
 * SPI Interfaces
 *
 * The radio has its own bus (SPIM2, set up by Rrptr from the variant's
 * board_config.h), normally D8 SCK, D9 MISO, D10 MOSI.
 *
 * The default SPI bus (SPIM3) is dedicated to the on-board P25Q16H flash. It
 * is wired for QSPI but used as plain SPI, so initVariant() holds its /WP and
 * /HOLD pins high.
 */
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_SCK          (21)      // P0.21 -> P25Q16H CLK
#define PIN_SPI_MOSI         (20)      // P0.20 -> P25Q16H DI  (IO0)
#define PIN_SPI_MISO         (24)      // P0.24 -> P25Q16H DO  (IO1)
#define PIN_EXTERNAL_FLASH_CS (25)     // P0.25 -> P25Q16H /CS
#define PIN_EXTERNAL_FLASH_WP (22)     // P0.22 -> P25Q16H /WP   (IO2)
#define PIN_EXTERNAL_FLASH_HOLD (23)   // P0.23 -> P25Q16H /HOLD (IO3)

static const uint8_t SS   = PIN_EXTERNAL_FLASH_CS ;
static const uint8_t MOSI = PIN_SPI_MOSI ;
static const uint8_t MISO = PIN_SPI_MISO ;
static const uint8_t SCK  = PIN_SPI_SCK ;

/*
 * Wire Interfaces
 */
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (4)       // P0.04 / D4
#define PIN_WIRE_SCL         (5)       // P0.05 / D5

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#endif

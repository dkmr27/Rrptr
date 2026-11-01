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

// ProMicro / nice!nano compatible nRF52840 board (Nologo ProMicro NRF52840),
// as used on FakeTec-style carrier boards. Arduino core view of the board only;
// the radio and other Rrptr hardware specifics are in each variant's
// board_config.h, so several variants (different radio modules) share this.
//
// Pin numbers are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx);
// the pin map in variant.cpp is identity. ProMicro "Dn" labels are given in
// the comments for reference when wiring.

#ifndef _VARIANT_PROMICRO_NRF52840_
#define _VARIANT_PROMICRO_NRF52840_

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
#define PIN_LED1             (15)      // P0.15, onboard LED
#define LED_BUILTIN          PIN_LED1
#define LED_RED              PIN_LED1
#define LED_BLUE             PIN_LED1
#define LED_STATE_ON         1         // State when LED is lit

// Buttons
#define PIN_BUTTON1          (32)      // P1.00 / D6

// External 3V3 (VCC) rail enable, powers radio and peripherals
#define PIN_VCC_EN           (13)      // P0.13

/*
 * Analog pins
 */
#define PIN_A0               (31)      // P0.31 / AIN7, battery voltage
#define PIN_A1               (2)       // P0.02 / AIN0
#define PIN_A2               (3)       // P0.03 / AIN1
#define PIN_A3               (4)       // P0.04 / AIN2
#define PIN_A4               (5)       // P0.05 / AIN3
#define PIN_A5               (28)      // P0.28 / AIN4
#define PIN_A6               (29)      // P0.29 / AIN5
#define PIN_A7               (30)      // P0.30 / AIN6

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

// NFC pins intentionally left undefined: on FakeTec-style boards P0.09 and
// P0.10 are radio RESET and DIO1. Variants set CONFIG_NFCT_PINS_AS_GPIOS.

/*
 * Serial interfaces
 */
#define PIN_SERIAL1_RX      (8)        // P0.08 / D1
#define PIN_SERIAL1_TX      (6)        // P0.06 / D0

/*
 * SPI Interfaces
 *
 * The radio module has its own bus (SPIM2, set up by Rrptr from the
 * variant's board_config.h); on FakeTec-style boards SCK P1.11, MOSI P1.15,
 * MISO P0.02, CS P1.13.
 *
 * The default SPI bus (SPIM3) is dedicated to the external W25Q128 flash.
 * W25Q128 /WP and /HOLD must be tied to 3V3.
 */
#define SPI_INTERFACES_COUNT 1

#define PIN_SPI_SCK          (22)      // P0.22 / D4 -> W25Q128 CLK
#define PIN_SPI_MOSI         (24)      // P0.24 / D5 -> W25Q128 DI  (IO0)
#define PIN_SPI_MISO         (38)      // P1.06 / D9 -> W25Q128 DO  (IO1)
#define PIN_EXTERNAL_FLASH_CS (20)     // P0.20 / D3 -> W25Q128 /CS

static const uint8_t SS   = PIN_EXTERNAL_FLASH_CS ;
static const uint8_t MOSI = PIN_SPI_MOSI ;
static const uint8_t MISO = PIN_SPI_MISO ;
static const uint8_t SCK  = PIN_SPI_SCK ;

/*
 * Wire Interfaces (OLED display header)
 */
#define WIRE_INTERFACES_COUNT 1

#define PIN_WIRE_SDA         (36)      // P1.04 / D8
#define PIN_WIRE_SCL         (11)      // P0.11 / D7

#ifdef __cplusplus
}
#endif

/*----------------------------------------------------------------------------
 *        Arduino objects - C++ only
 *----------------------------------------------------------------------------*/

#endif

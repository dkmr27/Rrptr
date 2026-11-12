// RAK3401 WisBlock core (nRF52840) with a RAK13302 1 W LoRa module (SX1262 and a
// Skyworks SKY66122-11 front end) in the IO slot, and a RAK15001 flash module,
// which Rrptr needs for its storage.
//
// Not yet tested on hardware. Two requirements:
//  - The RAK15001 must be in slot A. Its /WP and /HOLD are on the slot's IO lines,
//    and in the other slots those are the RAK13302's.
//  - The RAK13302 needs a 0 ohm resistor moved from R7 to R4, which puts the
//    radio's chip select on IO7 (P0.28). Otherwise it shares the flash's.
// Radio and flash then share the WisBlock SPI bus, each with its own chip select.
//
// Pins and front-end control as in MeshCore's rak3401 variant.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx). -1 = not fitted.

#pragma once

#define BOARD_NAME            "RAK3401 RAK13302"

// User I/O
#define BOARD_LED             35        // P1.03, green
#define BOARD_LED_ACTIVE_HIGH 1
#define BOARD_BUTTON          -1        // none, only reset

// Serial console on the base board's UART1 pins, alongside USB. -1 = none.
#define BOARD_UART_RX         15        // P0.15
#define BOARD_UART_TX         16        // P0.16

// IO2: the switched 3V3 rail and the RAK13302's 5 V boost for its PA. It must
// stay on while the radio is in use. Also driven high by the board's
// initVariant(), as the flash needs the rail.
#define BOARD_RAIL_EN         34        // P1.02, high = on

// IO3: the SKY66122's CSD and CPS, tied together on the RAK13302. High enables
// the front end (its LNA on receive, its PA on transmit); it cannot bypass the
// LNA on its own. Raised after the rail.
#define BOARD_PINS_HIGH       { 21 }    // P0.21

// Battery sense on AIN3, as the RAK4631 (calibrate with Battery Calibration)
#define BOARD_VBAT            5         // P0.05 / AIN3
#define BOARD_VBAT_DIVIDER    1.73f

// External flash: RAK15001 (GD25Q16C) in slot A, on the default SPI bus (SPIM3)
#define BOARD_HAS_EXT_FLASH   1
#define BOARD_FLASH_DEVICE    RAK15001  // chip definition from microStore's FlashFS adapter
#define BOARD_FLASH_NAME      "RAK15001"
#define BOARD_FLASH_SCK       3         // P0.03
#define BOARD_FLASH_MOSI      30        // P0.30
#define BOARD_FLASH_MISO      29        // P0.29
#define BOARD_FLASH_CS        26        // P0.26

// The radio is on the same bus as the flash
#define BOARD_RADIO_SHARES_FLASH_SPI 1

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            3         // P0.03, shared with the flash
#define RADIO0_MISO           29        // P0.29, shared with the flash
#define RADIO0_MOSI           30        // P0.30, shared with the flash
#define RADIO0_CS             28        // P0.28 / IO7, with R7 moved to R4
#define RADIO0_RESET          4         // P0.04 / IO4
#define RADIO0_BUSY           9         // P0.09 / IO5 (NFC pin)
#define RADIO0_DIO1           10        // P0.10 / IO6 (NFC pin)
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 drives the SKY66122's CTX
#define RADIO0_RXEN           -1
#define RADIO0_TXEN           -1
#define RADIO0_MAX_POWER_DBM  22        // chip drive ceiling, as MeshCore; the module is rated 1 W out
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
// Default for `rxgain`, which is MeshCore's `radio.lna`: off, as with the front
// end's own LNA a tester found about 3 dB better reception with it off
#define RADIO0_RX_BOOSTED_GAIN  0

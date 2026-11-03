// Promicro-SX1262 ("FakeTec"): Nologo ProMicro nRF52840 / nice!nano with an
// E22-style SX1262 module and a W25Q128 flash.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx). -1 = not fitted.

#pragma once

#define BOARD_NAME            "Promicro-SX1262"

// User I/O
#define BOARD_LED             15        // P0.15
#define BOARD_LED_ACTIVE_HIGH 1
#define BOARD_BUTTON          -1        // none fitted

// Serial console on UART pins, alongside USB (e.g. for power measurements
// with USB unplugged). -1 = none.
#define BOARD_UART_RX         8         // P0.08 / D1
#define BOARD_UART_TX         6         // P0.06 / D0

// Switched 3V3 rail for radio and flash
#define BOARD_RAIL_EN         13        // P0.13, high = on

// Battery sense: 1M / 1M divider, ADC sees half the battery voltage
#define BOARD_VBAT            31        // P0.31 / AIN7
#define BOARD_VBAT_DIVIDER    2.0f

// External flash: W25Q128 on the default SPI bus (SPIM3)
#define BOARD_HAS_EXT_FLASH   1
#define BOARD_FLASH_DEVICE    W25Q128   // chip definition from microStore's FlashFS adapter
#define BOARD_FLASH_NAME      "W25Q128"
#define BOARD_FLASH_SCK       22        // P0.22 / D4
#define BOARD_FLASH_MOSI      24        // P0.24 / D5
#define BOARD_FLASH_MISO      38        // P1.06 / D9
#define BOARD_FLASH_CS        20        // P0.20 / D3

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            43        // P1.11
#define RADIO0_MISO           2         // P0.02
#define RADIO0_MOSI           47        // P1.15
#define RADIO0_CS             45        // P1.13
#define RADIO0_RESET          9         // P0.09 (NFC pin, needs CONFIG_NFCT_PINS_AS_GPIOS)
#define RADIO0_BUSY           29        // P0.29
#define RADIO0_DIO1           10        // P0.10 (NFC pin)
#define RADIO0_TCXO_VOLTS     1.8f      // E22 TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 drives TXEN
#define RADIO0_RXEN           17        // P0.17
#define RADIO0_TXEN           -1        // driven by DIO2
#define RADIO0_MAX_POWER_DBM  22        // chip power ceiling
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`: ~3 dB better sensitivity, 0.45 mA more in RX (measured)

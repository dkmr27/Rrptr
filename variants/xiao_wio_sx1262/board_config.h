// Seeed XIAO nRF52840 + Wio-SX1262 kit.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx). -1 = not fitted.

#pragma once

#define BOARD_NAME            "XIAO nRF52840 Wio-SX1262"

// User I/O
#define BOARD_LED             26        // P0.26, red
#define BOARD_LED_ACTIVE_HIGH 0
#define BOARD_BUTTON          2         // P0.02 / D0, kit's user button

// Serial console on UART pins, alongside USB (e.g. for power measurements
// with USB unplugged). -1 = none.
#define BOARD_UART_RX         44        // P1.12 / D7
#define BOARD_UART_TX         43        // P1.11 / D6

// No switched rail
#define BOARD_RAIL_EN         -1

// Battery sense: 1M / 510k divider, ADC sees 0.338 of the battery voltage.
// Its enable pin (P0.14) is held low by the board's initVariant().
#define BOARD_VBAT            31        // P0.31 / AIN7
#define BOARD_VBAT_DIVIDER    2.961f

// On-board flash on the default SPI bus (SPIM3): QSPI wiring used as plain SPI,
// so as with microStore's own definitions QSPI is left off and the clock low.
#define P25Q16H_SPI                     \
{                                       \
	.total_size = (1UL << 21),          \
	.start_up_time_us = 5000,           \
	.manufacturer_id = 0x85,            \
	.memory_type = 0x60,                \
	.capacity = 0x15,                   \
	.max_clock_speed_mhz = 8,           \
	.quad_enable_bit_mask = 0x00,       \
	.has_sector_protection = false,     \
	.supports_fast_read = true,         \
	.supports_qspi = false,             \
	.supports_qspi_writes = false,      \
	.write_status_register_split = false, \
	.single_status_byte = false,        \
	.is_fram = false,                   \
}

#define BOARD_HAS_EXT_FLASH   1
#define BOARD_FLASH_DEVICE    P25Q16H_SPI
#define BOARD_FLASH_NAME      "P25Q16H"
#define BOARD_FLASH_SCK       21        // P0.21
#define BOARD_FLASH_MOSI      20        // P0.20
#define BOARD_FLASH_MISO      24        // P0.24
#define BOARD_FLASH_CS        25        // P0.25

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            45        // P1.13 / D8
#define RADIO0_MISO           46        // P1.14 / D9
#define RADIO0_MOSI           47        // P1.15 / D10
#define RADIO0_CS             4         // P0.04 / D4
#define RADIO0_RESET          28        // P0.28 / D2
#define RADIO0_BUSY           29        // P0.29 / D3
#define RADIO0_DIO1           3         // P0.03 / D1
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 drives TX on the RF switch
#define RADIO0_RXEN           5         // P0.05 / D5
#define RADIO0_TXEN           -1        // driven by DIO2
#define RADIO0_MAX_POWER_DBM  22        // chip power ceiling
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

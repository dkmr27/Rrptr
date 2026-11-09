// Seeed XIAO nRF52840 on the Xiao-LoRa2021F33-2G4 board: a NiceRF LoRa2021F33
// module, an LR2021 with a PA on both bands
// (https://github.com/dkmr27/Xiao-LoRa2021F33-2G4). The board only wires the
// radio: no button or LED of its own.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx). -1 = not fitted.

#pragma once

#define BOARD_NAME            "XIAO nRF52840 LR2021"

// User I/O
#define BOARD_LED             26        // P0.26, red
#define BOARD_LED_ACTIVE_HIGH 0
#define BOARD_BUTTON          -1        // none on this board

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

#define RADIO0_CHIP           RADIO_CHIP_LR2021
#define RADIO0_SCK            45        // P1.13 / D8
#define RADIO0_MISO           46        // P1.14 / D9
#define RADIO0_MOSI           47        // P1.15 / D10
#define RADIO0_CS             4         // P0.04 / D4
#define RADIO0_RESET          28        // P0.28 / D2
#define RADIO0_BUSY           29        // P0.29 / D3
#define RADIO0_DIO1           3         // P0.03 / D1, the LR2021's DIO9 IRQ
#define RADIO0_TCXO_VOLTS     3.3f      // TCXO; the driver falls back to a crystal
#define RADIO0_DIO2_RF_SWITCH 0
#define RADIO0_RXEN           -1        // RF switch on the LR2021's own DIOs, below
#define RADIO0_TXEN           -1
// Drive ceilings, the module's PA gain not included. 2.4 GHz: HARD LIMIT, 0 dBm
// already gives about 30 dBm out and more drive can damage the module.
// Sub-GHz: 18 dBm gives almost full output.
#define RADIO0_MAX_POWER_DBM  22
#define RADIO0_MAX_POWER_2G4_DBM 0
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

// RF switch on LR2021 DIO5-DIO8, levels in that order per mode. Expanded only in
// the LR2021 driver, which has RadioLib.
#define RADIO0_RF_SWITCH_PINS  { RADIOLIB_LR2021_DIO5, RADIOLIB_LR2021_DIO6, \
                                 RADIOLIB_LR2021_DIO7, RADIOLIB_LR2021_DIO8 }
#define RADIO0_RF_SWITCH_MODES { \
	{ LR2021::MODE_STBY,  { LOW,  LOW,  LOW,  LOW  } }, \
	{ LR2021::MODE_RX,    { LOW,  LOW,  LOW,  LOW  } }, \
	{ LR2021::MODE_TX,    { LOW,  HIGH, LOW,  LOW  } }, \
	{ LR2021::MODE_RX_HF, { HIGH, LOW,  HIGH, LOW  } }, \
	{ LR2021::MODE_TX_HF, { LOW,  LOW,  HIGH, HIGH } }, \
	END_OF_MODE_TABLE }

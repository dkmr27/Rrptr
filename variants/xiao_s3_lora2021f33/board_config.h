// Seeed XIAO ESP32-S3 (8 MB flash) on the Xiao-LoRa2021F33-2G4 board: a NiceRF
// LoRa2021F33 module, an LR2021 with a PA on both bands
// (https://github.com/dkmr27/Xiao-LoRa2021F33-2G4). The board only wires the
// radio: no button or LED of its own.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are ESP32-S3 GPIO numbers. -1 = not fitted.

#pragma once

#define BOARD_NAME            "XIAO ESP32-S3 LR2021"

// User I/O
#define BOARD_LED             21        // XIAO's own user LED
#define BOARD_LED_ACTIVE_HIGH 0
#define BOARD_BUTTON          0         // XIAO's BOOT button, active low

// Serial console on UART pins, alongside USB. USB stops during
// light sleep, so this is the console for battery use.
#define BOARD_UART_RX         44        // D7
#define BOARD_UART_TX         43        // D6

// Longest main loop idle: each timed wake from light sleep costs more here,
// as measured on the T3S3
#define BOARD_LOOP_IDLE_MS    200

// No switched rail
#define BOARD_RAIL_EN         -1

// No battery sense on the base XIAO ESP32-S3
#define BOARD_VBAT            -1

// Storage: LittleFS on the ESP32-S3's own flash
#define BOARD_HAS_EXT_FLASH   0

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_LR2021
#define RADIO0_SCK            7         // D8
#define RADIO0_MISO           8         // D9
#define RADIO0_MOSI           9         // D10
#define RADIO0_CS             5         // D4
#define RADIO0_RESET          3         // D2
#define RADIO0_BUSY           4         // D3
#define RADIO0_DIO1           2         // D1, the LR2021's DIO9 IRQ
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

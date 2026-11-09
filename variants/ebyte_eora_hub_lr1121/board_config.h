// Ebyte EoRa-HUB-900TB: ESP32-S3 (4 MB flash, 2 MB PSRAM), Ebyte E80 module
// (LR1121, sub-GHz and 2.4 GHz, no external PA), SSD1306 OLED, CH340 USB serial.
// Pins as in MeshCore's ebyte_eora_hub variant.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are ESP32-S3 GPIO numbers. -1 = not fitted.

#pragma once

#define BOARD_NAME            "Ebyte EoRa-HUB"

// User I/O
#define BOARD_LED             35        // blue user LED
#define BOARD_LED_ACTIVE_HIGH 1
#define BOARD_BUTTON          0         // PRG button, active low

// The board's USB port is a CH340 on these pins, so this is the console over USB
// as well. It stops during light sleep: press Enter to wake the node, the first
// characters are lost.
#define BOARD_UART_RX         44
#define BOARD_UART_TX         43

// Longest main loop idle: each timed wake from light sleep costs more here,
// as measured on the T3S3
#define BOARD_LOOP_IDLE_MS    200

// No switched rail
#define BOARD_RAIL_EN         -1

// Battery sense, with an enable. Its reading looks wrong on this board, possibly
// a hardware fault; the divider is assumed to be the Heltec V3's.
#define BOARD_VBAT            1
#define BOARD_VBAT_EN         37
#define BOARD_VBAT_DIVIDER    4.9f

// Storage: LittleFS on the ESP32-S3's own flash
#define BOARD_HAS_EXT_FLASH   0

// OLED (SSD1306, I2C)
#define BOARD_HAS_DISPLAY     1
#define BOARD_OLED_WIDTH      128
#define BOARD_OLED_HEIGHT     64
#define BOARD_OLED_ADDRESS    0x3C
#define BOARD_OLED_SDA        18
#define BOARD_OLED_SCL        17
#define BOARD_OLED_RESET      21

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_LR1121
#define RADIO0_SCK            9
#define RADIO0_MISO           11
#define RADIO0_MOSI           10
#define RADIO0_CS             8
#define RADIO0_RESET          12
#define RADIO0_BUSY           13
#define RADIO0_DIO1           14        // the LR1121's DIO9 IRQ
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO in the E80
#define RADIO0_DIO2_RF_SWITCH 0
#define RADIO0_RXEN           -1        // RF switch on the LR1121's own DIOs, below
#define RADIO0_TXEN           -1
#define RADIO0_MAX_POWER_DBM  22        // sub-GHz drive ceiling
#define RADIO0_MAX_POWER_2G4_DBM 13     // 2.4 GHz PA maximum, no external PA
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

// RF switch on LR1121 DIO5 and DIO6, levels in that order per mode. Expanded only
// in the LR1121 driver, which has RadioLib.
#define RADIO0_RF_SWITCH_PINS  { RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6 }
#define RADIO0_RF_SWITCH_MODES { \
	{ LR11x0::MODE_STBY,  { LOW,  LOW  } }, \
	{ LR11x0::MODE_RX,    { LOW,  HIGH } }, \
	{ LR11x0::MODE_TX,    { HIGH, HIGH } }, \
	{ LR11x0::MODE_TX_HP, { HIGH, LOW  } }, \
	{ LR11x0::MODE_TX_HF, { LOW,  LOW  } }, \
	{ LR11x0::MODE_GNSS,  { LOW,  LOW  } }, \
	{ LR11x0::MODE_WIFI,  { LOW,  LOW  } }, \
	END_OF_MODE_TABLE }

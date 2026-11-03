// LilyGO T3S3 V1.2 with SX1262: ESP32-S3 (4 MB flash, 2 MB PSRAM), SSD1306
// OLED, SD card slot.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are ESP32-S3 GPIO numbers. -1 = not fitted.

#pragma once

#define BOARD_NAME            "LilyGO T3S3 SX1262"

// User I/O
#define BOARD_LED             37
#define BOARD_LED_ACTIVE_HIGH 1
#define BOARD_BUTTON          0         // BOOT button, active low

// Serial console: native USB only
#define BOARD_UART_RX         -1
#define BOARD_UART_TX         -1

// No switched rail
#define BOARD_RAIL_EN         -1

// Battery sense: 100k / 100k divider, ADC sees half the battery voltage
#define BOARD_VBAT            1
#define BOARD_VBAT_DIVIDER    2.0f

// Storage: LittleFS on the ESP32-S3's own flash
#define BOARD_HAS_EXT_FLASH   0

// OLED (SSD1306, I2C); not used yet
#define BOARD_OLED_SDA        18
#define BOARD_OLED_SCL        17
#define BOARD_OLED_RESET      21

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            5
#define RADIO0_MISO           3
#define RADIO0_MOSI           6
#define RADIO0_CS             7
#define RADIO0_RESET          8
#define RADIO0_BUSY           34
#define RADIO0_DIO1           33
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 drives the RF switch
#define RADIO0_RXEN           -1
#define RADIO0_TXEN           -1
#define RADIO0_MAX_POWER_DBM  22        // chip power ceiling
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

// Seeed XIAO ESP32-S3 (8 MB flash) + SX1262, with the radio on the outer pins, wired
// as on the XIAO nRF52840 kit (xiao_nrf_sx1262) rather than by the B2B connector.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are ESP32-S3 GPIO numbers. -1 = not fitted.

#pragma once

#define BOARD_NAME            "XIAO ESP32-S3 SX1262 alt"

// User I/O
#define BOARD_LED             21        // XIAO's own user LED
#define BOARD_LED_ACTIVE_HIGH 0
#define BOARD_BUTTON          1         // D0, user button, active low

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

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            7         // D8
#define RADIO0_MISO           8         // D9
#define RADIO0_MOSI           9         // D10
#define RADIO0_CS             5         // D4
#define RADIO0_RESET          3         // D2
#define RADIO0_BUSY           4         // D3
#define RADIO0_DIO1           2         // D1
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 drives TX on the RF switch
#define RADIO0_RXEN           6         // D5
#define RADIO0_TXEN           -1        // driven by DIO2
#define RADIO0_MAX_POWER_DBM  22        // chip power ceiling
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

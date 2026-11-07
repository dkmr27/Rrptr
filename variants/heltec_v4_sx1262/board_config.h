// Heltec WiFi LoRa 32 V4: ESP32-S3 (16 MB flash, 2 MB PSRAM), SX1262 with a
// front-end module (V4.2 GC1109, V4.3 KCT8103L), SSD1315 OLED. Not the V4R8.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are ESP32-S3 GPIO numbers. -1 = not fitted.

#pragma once

#define BOARD_NAME            "Heltec V4"

// User I/O
#define BOARD_LED             35
#define BOARD_LED_ACTIVE_HIGH 1
#define BOARD_BUTTON          0         // BOOT button, active low

// Serial console on free header pins, alongside USB. USB stops
// during light sleep, so this is the console for battery use.
#define BOARD_UART_RX         44
#define BOARD_UART_TX         43

// Longest main loop idle: each timed wake from light sleep costs more here,
// as measured on the T3S3
#define BOARD_LOOP_IDLE_MS    200

// Vext, which powers the OLED. Sources disagree on its polarity: Meshtastic and
// the V4.3 schematic say active low, MeshCore says active high.
#define BOARD_RAIL_EN         36
#define BOARD_RAIL_ACTIVE_HIGH 0

// Front-end module, held on from boot: FEM supply (7), chip enable (2), and on
// the GC1109 the PA (46), which only matters while transmitting. DIO2 switches
// it between receive and transmit; on the KCT8103L, TXEN does the rest.
#define BOARD_PINS_HIGH       { 7, 2, 46 }

// Battery sense: 390k / 100k divider, ADC sees 0.204 of the battery voltage
#define BOARD_VBAT            1
#define BOARD_VBAT_EN         37
#define BOARD_VBAT_DIVIDER    4.9f

// Storage: LittleFS on the ESP32-S3's own flash
#define BOARD_HAS_EXT_FLASH   0

// OLED (SSD1315, SSD1306 compatible, I2C)
#define BOARD_HAS_DISPLAY     1
#define BOARD_OLED_WIDTH      128
#define BOARD_OLED_HEIGHT     64
#define BOARD_OLED_ADDRESS    0x3C
#define BOARD_OLED_SDA        17
#define BOARD_OLED_SCL        18
#define BOARD_OLED_RESET      21

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            9
#define RADIO0_MISO           11
#define RADIO0_MOSI           10
#define RADIO0_CS             8
#define RADIO0_RESET          12
#define RADIO0_BUSY           13
#define RADIO0_DIO1           14
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 switches the front-end module
#define RADIO0_RXEN           -1
#define RADIO0_TXEN           5         // KCT8103L CTX: high to transmit, low for its receive LNA
#define RADIO0_MAX_POWER_DBM  22        // chip power ceiling; the front-end module adds about 7-11 dB
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

// RAK4631 WisBlock core (nRF52840 + SX1262) on a RAK19007 base, with a RAK15001
// flash module, which Rrptr needs for its storage. In any sensor slot.
//
// Not yet tested on hardware. Pins are from RAK's documentation, MeshCore and
// microReticulum_Firmware, which agree.
//
// Pins, radio type and other hardware specifics for this variant. Pin numbers
// are raw nRF52840 GPIO numbers (P0.xx = xx, P1.xx = 32 + xx). -1 = not fitted.

#pragma once

#define BOARD_NAME            "RAK4631 RAK15001"

// User I/O
#define BOARD_LED             35        // P1.03, green
#define BOARD_LED_ACTIVE_HIGH 1
#define BOARD_BUTTON          -1        // none, only reset

// Serial console on the base board's UART1 pins, alongside USB. -1 = none.
#define BOARD_UART_RX         15        // P0.15
#define BOARD_UART_TX         16        // P0.16

// Powers the antenna switch, so it stays high during transmit too. It is not an
// RXEN pin: RadioLib would take it low to transmit.
#define BOARD_RAIL_EN         37        // P1.05, high = on

// Battery sense on AIN3. Others use 1.73 with a 3.0 V ADC reference and Rrptr uses
// 3.6 V, so this may want trimming with the Battery Calibration setting.
#define BOARD_VBAT            5         // P0.05 / AIN3
#define BOARD_VBAT_DIVIDER    1.73f

// External flash: RAK15001 (GD25Q16C) on the default SPI bus (SPIM3). Its /WP and
// /HOLD and the switched 3V3 rail are driven high by the board's initVariant().
#define BOARD_HAS_EXT_FLASH   1
#define BOARD_FLASH_DEVICE    RAK15001  // chip definition from microStore's FlashFS adapter
#define BOARD_FLASH_NAME      "RAK15001"
#define BOARD_FLASH_SCK       3         // P0.03
#define BOARD_FLASH_MOSI      30        // P0.30
#define BOARD_FLASH_MISO      29        // P0.29
#define BOARD_FLASH_CS        26        // P0.26

// Radios
#define RADIO_COUNT           1

#define RADIO0_CHIP           RADIO_CHIP_SX1262
#define RADIO0_SCK            43        // P1.11
#define RADIO0_MISO           45        // P1.13
#define RADIO0_MOSI           44        // P1.12
#define RADIO0_CS             42        // P1.10
#define RADIO0_RESET          38        // P1.06
#define RADIO0_BUSY           46        // P1.14
#define RADIO0_DIO1           47        // P1.15
#define RADIO0_TCXO_VOLTS     1.8f      // TCXO on DIO3
#define RADIO0_DIO2_RF_SWITCH 1         // DIO2 drives the RF switch
#define RADIO0_RXEN           -1        // see BOARD_RAIL_EN
#define RADIO0_TXEN           -1        // driven by DIO2
#define RADIO0_MAX_POWER_DBM  22        // chip power ceiling
#define RADIO0_CURRENT_LIMIT_MA 140     // PA over-current limit needed for 22 dBm
#define RADIO0_RX_BOOSTED_GAIN  1       // default for `rxgain`

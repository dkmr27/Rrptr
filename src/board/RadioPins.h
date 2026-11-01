// Radio wiring, gathered from the RADIOn_* defines in board_config.h so the
// board and radio code can loop over the fitted radios.

#pragma once

#include <stdint.h>

#define RADIO_CHIP_SX1262 1
#define RADIO_CHIP_LR1121 2
#define RADIO_CHIP_SX1280 3
#define RADIO_CHIP_LR2021 4

#include "board_config.h"

#ifndef RADIO_COUNT
	#error "board_config.h must define RADIO_COUNT"
#endif

struct RadioPins {
	uint8_t chip;               // RADIO_CHIP_*
	int8_t  sck, miso, mosi;
	int8_t  cs, reset, busy, dio1;
	float   tcxo_volts;         // 0 = crystal
	bool    dio2_rf_switch;
	int8_t  rxen, txen;         // -1 = not used
	int8_t  max_power_dbm;
};

#define RADIO_PINS(n) { \
	RADIO##n##_CHIP, \
	RADIO##n##_SCK, RADIO##n##_MISO, RADIO##n##_MOSI, \
	RADIO##n##_CS, RADIO##n##_RESET, RADIO##n##_BUSY, RADIO##n##_DIO1, \
	RADIO##n##_TCXO_VOLTS, RADIO##n##_DIO2_RF_SWITCH, \
	RADIO##n##_RXEN, RADIO##n##_TXEN, \
	RADIO##n##_MAX_POWER_DBM }

static const RadioPins radio_pins[RADIO_COUNT] = {
	RADIO_PINS(0),
#if RADIO_COUNT > 1
	RADIO_PINS(1),
#endif
};

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

// True if any fitted radio is `chip`, so unused drivers are not compiled
#define RADIO_USES(chip) (RADIO0_CHIP == (chip) || (RADIO_COUNT > 1 && RADIO1_CHIP == (chip)))

// Optional settings: 0 leaves the chip's default
#ifndef RADIO0_CURRENT_LIMIT_MA
	#define RADIO0_CURRENT_LIMIT_MA 0
#endif
#ifndef RADIO0_RX_BOOSTED_GAIN
	#define RADIO0_RX_BOOSTED_GAIN 0
#endif
// 2.4 GHz drive ceiling, on chips with that band. Unset: the band is not used.
#ifndef RADIO0_MAX_POWER_2G4_DBM
	#define RADIO0_MAX_POWER_2G4_DBM (-128)
#endif
#ifndef RADIO1_MAX_POWER_2G4_DBM
	#define RADIO1_MAX_POWER_2G4_DBM (-128)
#endif
// 1 = TXEN also bypasses a front-end module's receive LNA: held high while
// receiving, the LNA is bypassed (the FEM LNA setting)
#ifndef RADIO0_TXEN_LNA_BYPASS
	#define RADIO0_TXEN_LNA_BYPASS 0
#endif
#ifndef RADIO1_TXEN_LNA_BYPASS
	#define RADIO1_TXEN_LNA_BYPASS 0
#endif
#ifndef RADIO1_CURRENT_LIMIT_MA
	#define RADIO1_CURRENT_LIMIT_MA 0
#endif
#ifndef RADIO1_RX_BOOSTED_GAIN
	#define RADIO1_RX_BOOSTED_GAIN 0
#endif

struct RadioPins {
	uint8_t chip;               // RADIO_CHIP_*
	int8_t  sck, miso, mosi;
	int8_t  cs, reset, busy, dio1;
	float   tcxo_volts;         // 0 = crystal
	bool    dio2_rf_switch;
	int8_t  rxen, txen;         // -1 = not used
	int8_t  max_power_dbm;      // sub-GHz drive ceiling
	int8_t  max_power_2g4_dbm;  // 2.4 GHz drive ceiling, -128 = band not used
	uint8_t current_limit_ma;   // PA over-current limit, 0 = chip default
	bool    rx_boosted_gain;   // default for the RX boosted gain setting
	bool    txen_lna_bypass;   // see RADIOn_TXEN_LNA_BYPASS
};

#define RADIO_PINS(n) { \
	RADIO##n##_CHIP, \
	RADIO##n##_SCK, RADIO##n##_MISO, RADIO##n##_MOSI, \
	RADIO##n##_CS, RADIO##n##_RESET, RADIO##n##_BUSY, RADIO##n##_DIO1, \
	RADIO##n##_TCXO_VOLTS, RADIO##n##_DIO2_RF_SWITCH, \
	RADIO##n##_RXEN, RADIO##n##_TXEN, \
	RADIO##n##_MAX_POWER_DBM, RADIO##n##_MAX_POWER_2G4_DBM, \
	RADIO##n##_CURRENT_LIMIT_MA, RADIO##n##_RX_BOOSTED_GAIN, \
	RADIO##n##_TXEN_LNA_BYPASS }

static const RadioPins radio_pins[RADIO_COUNT] = {
	RADIO_PINS(0),
#if RADIO_COUNT > 1
	RADIO_PINS(1),
#endif
};

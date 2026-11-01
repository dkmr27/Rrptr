// Chip-independent band and power helpers.
//
// Pure functions with no Arduino or RadioLib dependency. Each radio driver
// supplies a table of what its chip can do per band; the board's ceiling is
// applied on top. Adapted from the radio HAL in openHop_modem (dad650f), by the
// openHop_modem project and its contributors.

#pragma once

#include <stdint.h>

enum class RadioBand : uint8_t {
	SubGhz  = 0,
	Band2G4 = 1,
};

// Anything at or above 1 GHz is treated as the 2.4 GHz band
constexpr uint32_t RADIO_BAND_2G4_MIN_HZ = 1000000000UL;

// Power value meaning "this band is not usable on this board"
constexpr int8_t RADIO_POWER_NONE = INT8_MIN;

inline RadioBand radio_band_for(uint32_t freq_hz) {
	return freq_hz >= RADIO_BAND_2G4_MIN_HZ ? RadioBand::Band2G4 : RadioBand::SubGhz;
}

// Highest drive level allowed on a band: the lower of the chip's limit and the
// board's ceiling. A board ceiling of RADIO_POWER_NONE disables the band.
inline int8_t radio_power_ceiling(int8_t chip_max_dbm, int8_t board_ceiling_dbm) {
	if (board_ceiling_dbm == RADIO_POWER_NONE) return RADIO_POWER_NONE;
	return board_ceiling_dbm < chip_max_dbm ? board_ceiling_dbm : chip_max_dbm;
}

inline int8_t radio_clamp_power(int8_t requested_dbm, int8_t ceiling_dbm) {
	return requested_dbm > ceiling_dbm ? ceiling_dbm : requested_dbm;
}

// What a chip can do on one band. Board ceilings are applied separately.
struct RadioBandInfo {
	RadioBand band;
	uint32_t freq_min_hz;
	uint32_t freq_max_hz;
	int8_t   power_min_dbm;     // chip drive range on this band
	int8_t   power_max_dbm;
	uint8_t  sf_min, sf_max;
	uint8_t  cr_min, cr_max;    // coding-rate denominator, 5 = 4/5
};

// LoRa settings for one radio
struct RadioConfig {
	uint32_t freq_hz;
	uint32_t bandwidth_hz;
	uint8_t  sf;
	uint8_t  cr;                // 5 to 8
	int8_t   power_dbm;         // chip power, clamped to the ceiling
	uint8_t  sync_word;
	uint16_t preamble_len;
};

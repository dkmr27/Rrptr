#include "radio/Radio.h"
#include "board/Board.h"

#if RADIO_USES(RADIO_CHIP_SX1262)
#include "radio/sx126x/SX1262Radio.h"
#endif

// ─── Interrupts ─────────────────────────────────────────────
// RadioLib takes a plain function for DIO1, so each radio slot gets its own.
// Each also wakes the main loop so RX/TX done is handled straight away.
// On ESP32 GPIO interrupts run from IRAM, also while the flash cache is off
// (e.g. during file system writes), so the handlers must live in IRAM too.
#if defined(RRPTR_MCU_ESP32)
	#define RADIO_ISR IRAM_ATTR
#else
	#define RADIO_ISR
#endif

static volatile bool irq_flag[RADIO_COUNT];

static void RADIO_ISR radio0_irq() { irq_flag[0] = true; board.wake(); }
#if RADIO_COUNT > 1
static void RADIO_ISR radio1_irq() { irq_flag[1] = true; board.wake(); }
#endif

void (*Radio::irq_handler() const)(void) {
#if RADIO_COUNT > 1
	if (_index == 1) return radio1_irq;
#endif
	return radio0_irq;
}

// ─── Factory ────────────────────────────────────────────────
Radio* radio_create(uint8_t index, SPIClass& spi) {
	if (index >= RADIO_COUNT) return nullptr;
	const RadioPins& pins = radio_pins[index];
	switch (pins.chip) {
#if RADIO_USES(RADIO_CHIP_SX1262)
	case RADIO_CHIP_SX1262: return new SX1262Radio(index, pins, spi);
#endif
	default: return nullptr;
	}
}

// ─── Capabilities ───────────────────────────────────────────
int8_t Radio::board_ceiling(RadioBand band) const {
	// Boards only state a sub-GHz ceiling so far; 2.4 GHz gets one with the first
	// dual-band board.
	return band == RadioBand::SubGhz ? _pins.max_power_dbm : RADIO_POWER_NONE;
}

const RadioBandInfo* Radio::band_info(RadioBand band) const {
	const RadioBandInfo* table = nullptr;
	uint8_t count = bands(table);
	for (uint8_t i = 0; i < count; i++) {
		if (table[i].band == band) return &table[i];
	}
	return nullptr;
}

int8_t Radio::max_output_power(uint32_t freq_hz) const {
	RadioBand band = radio_band_for(freq_hz);
	const RadioBandInfo* info = band_info(band);
	if (!info) return RADIO_POWER_NONE;
	return radio_power_ceiling(info->power_max_dbm, board_ceiling(band));
}

bool Radio::config_supported(const RadioConfig& c) const {
	RadioBand band = radio_band_for(c.freq_hz);
	const RadioBandInfo* info = band_info(band);
	if (!info) return false;
	if (max_output_power(c.freq_hz) == RADIO_POWER_NONE) return false;
	if (c.freq_hz < info->freq_min_hz || c.freq_hz > info->freq_max_hz) return false;
	if (c.sf < info->sf_min || c.sf > info->sf_max) return false;
	if (c.cr < info->cr_min || c.cr > info->cr_max) return false;
	if (!accepts_bandwidth(band, c.bandwidth_hz)) return false;
	if (!accepts_preamble(c.preamble_len)) return false;
	if (c.power_dbm < info->power_min_dbm) return false;
	return true;
}

// ─── Configuration ──────────────────────────────────────────
int16_t Radio::configure(const RadioConfig& c) {
	if (!config_supported(c)) return RADIOLIB_ERR_UNSUPPORTED;
	int16_t state = standby();
	if (state != RADIOLIB_ERR_NONE) return state;
	state = chip_set_modem(c);
	if (state != RADIOLIB_ERR_NONE) return state;
	_config = c;
	state = set_frequency(c.freq_hz);
	if (state != RADIOLIB_ERR_NONE) return state;
	return set_output_power(c.power_dbm);
}

int16_t Radio::set_frequency(uint32_t freq_hz) {
	int16_t state = chip_set_frequency(freq_hz);
	if (state != RADIOLIB_ERR_NONE) return state;
	bool band_changed = radio_band_for(freq_hz) != radio_band_for(_config.freq_hz);
	_config.freq_hz = freq_hz;
	// The drive level is only valid for the band it was clamped for
	if (band_changed) return apply_output_power();
	return RADIOLIB_ERR_NONE;
}

int16_t Radio::set_output_power(int8_t power_dbm) {
	_requested_power_dbm = power_dbm;
	return apply_output_power();
}

int16_t Radio::apply_output_power() {
	int8_t ceiling = max_output_power(_config.freq_hz);
	if (ceiling == RADIO_POWER_NONE) return RADIOLIB_ERR_INVALID_OUTPUT_POWER;
	int8_t power = radio_clamp_power(_requested_power_dbm, ceiling);
	// After a band change the old request can be below the new band's minimum
	const RadioBandInfo* info = band_info(radio_band_for(_config.freq_hz));
	if (info && power < info->power_min_dbm) power = info->power_min_dbm;
	int16_t state = chip_set_output_power(power);
	if (state == RADIOLIB_ERR_NONE) {
		_applied_power_dbm = power;
		_config.power_dbm = power;
	}
	return state;
}

// ─── Receive / transmit ─────────────────────────────────────
int16_t Radio::start_receive() {
	irq_flag[_index] = false;
	_transmitting = false;
	return chip_start_receive();
}

int16_t Radio::start_transmit(const uint8_t* data, size_t len) {
	irq_flag[_index] = false;
	int16_t state = chip_start_transmit(data, len);
	_transmitting = (state == RADIOLIB_ERR_NONE);
	if (!_transmitting) start_receive();
	return state;
}

Radio::Event Radio::poll() {
#if defined(RRPTR_MCU_ESP32)
	// An edge during light sleep raises no interrupt, so also check the level
	if (digitalRead(_pins.dio1) == HIGH) irq_flag[_index] = true;
#endif
	if (!irq_flag[_index]) return EVENT_NONE;
	irq_flag[_index] = false;

	if (_transmitting) {
		chip_finish_transmit();
		start_receive();
		return EVENT_TX_DONE;
	}
	return EVENT_RX_DONE;
}

int16_t Radio::read_packet(uint8_t* buf, size_t max_len) {
	size_t len = chip_packet_length();
	if (len > max_len) len = max_len;
	int16_t state = chip_read(buf, len);
	// The chip only checks the payload CRC when the received header says there
	// is one. A header corrupted into "no CRC" therefore passes as a good
	// packet. Every RNode-format frame is sent with CRC, so drop those.
	if (state == RADIOLIB_ERR_NONE && !chip_rx_header_has_crc()) state = RADIO_ERR_NO_CRC;
	_rssi = chip_packet_rssi();
	_snr = chip_packet_snr();
	chip_continue_receive();
	return state == RADIOLIB_ERR_NONE ? (int16_t)len : state;
}

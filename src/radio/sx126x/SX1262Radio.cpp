// SX1262 driver.
//
// Ported from the radio HAL in openHop_modem (https://github.com/dkmr27/openhop_modem,
// branch radio-hal, dad650f), MIT licence, by the openHop_modem project and its
// contributors. receiving_packet() is modelled on MeshCore's
// CustomSX1262::isReceiving(). The licence notice is in licenses/openhop_modem.txt.

#include "radio/sx126x/SX1262Radio.h"

#if RADIO_USES(RADIO_CHIP_SX1262)

// Limits match RadioLib's own checks for the SX1262 (setFrequency,
// setOutputPower, setSpreadingFactor, setCodingRate)
static const RadioBandInfo SX1262_BANDS[] = {
	{ RadioBand::SubGhz, 150000000UL, 960000000UL, -9, 22, 5, 12, 5, 8 },
};

// Longest LoRa frame, for the receive guard's staleness bound
static const size_t MAX_FRAME_LEN = 255;

SX1262Radio::SX1262Radio(uint8_t index, const RadioPins& pins, SPIClass& spi)
	: Radio(index, pins),
	  _chip(new Module(pins.cs, pins.dio1, pins.reset, pins.busy, spi)) {}

int16_t SX1262Radio::begin(const RadioConfig& c) {
	int16_t state = _chip.begin(c.freq_hz / 1e6f, c.bandwidth_hz / 1e3f, c.sf, c.cr,
		c.sync_word, 0, c.preamble_len, _pins.tcxo_volts, false);
	if (state != RADIOLIB_ERR_NONE) return state;

	if (_pins.dio2_rf_switch) {
		state = _chip.setDio2AsRfSwitch(true);
		if (state != RADIOLIB_ERR_NONE) return state;
	}

	// Set the RF switching pins if required
	if (_pins.rxen >= 0 || _pins.txen >= 0) {
		_chip.setRfSwitchPins(_pins.rxen >= 0 ? (uint32_t)_pins.rxen : RADIOLIB_NC,
		                      _pins.txen >= 0 ? (uint32_t)_pins.txen : RADIOLIB_NC);
	}
	if (_pins.rx_boosted_gain) {
		state = _chip.setRxBoostedGainMode(true);
		if (state != RADIOLIB_ERR_NONE) return state;
	}
	_chip.setDio1Action(irq_handler());

	return configure(c);
}

uint8_t SX1262Radio::bands(const RadioBandInfo*& table) const {
	table = SX1262_BANDS;
	return sizeof(SX1262_BANDS) / sizeof(SX1262_BANDS[0]);
}

bool SX1262Radio::accepts_bandwidth(RadioBand band, uint32_t bw_hz) const {
	if (band != RadioBand::SubGhz) return false;
	// Same test as SX126x::setBandwidth(), which buckets bw/2 rather than
	// matching exact values
	float bw = bw_hz / 1000.0f;
	if (bw < 0.0f || bw > 510.0f) return false;
	switch ((uint8_t)(bw / 2 + 0.01f)) {
	case 3: case 5: case 7: case 10: case 15: case 20:
	case 31: case 62: case 125: case 250:
		return true;
	default:
		return false;
	}
}

// ─── Configuration ──────────────────────────────────────────
int16_t SX1262Radio::standby() { return _chip.standby(); }

int16_t SX1262Radio::chip_set_frequency(uint32_t freq_hz) {
	return _chip.setFrequency(freq_hz / 1e6f);
}

int16_t SX1262Radio::chip_set_output_power(int8_t power_dbm) {
	int16_t state = _chip.setOutputPower(power_dbm);
	if (state != RADIOLIB_ERR_NONE || _pins.current_limit_ma == 0) return state;
	return _chip.setCurrentLimit(_pins.current_limit_ma);
}

int16_t SX1262Radio::chip_set_modem(const RadioConfig& c) {
	int16_t state;
	if ((state = _chip.setBandwidth(c.bandwidth_hz / 1e3f)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setSpreadingFactor(c.sf)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setCodingRate(c.cr)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setSyncWord(c.sync_word)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setPreambleLength(c.preamble_len)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.explicitHeader()) != RADIOLIB_ERR_NONE) return state;
	return _chip.setCRC(2);
}

// ─── Receive / transmit ─────────────────────────────────────
int16_t SX1262Radio::chip_start_receive() {
	_rx_activity_at = 0;
	_rx_header_seen = false;
	// RadioLib's default receive IRQs leave out PREAMBLE_DETECTED; latch it so
	// receiving_packet() can see a frame from its preamble on
	return _chip.startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF,
		RADIOLIB_IRQ_RX_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED),
		RADIOLIB_IRQ_RX_DEFAULT_MASK, 0);
}

int16_t SX1262Radio::chip_start_transmit(const uint8_t* data, size_t len) {
	return _chip.startTransmit(data, len);
}

int16_t SX1262Radio::chip_finish_transmit() { return _chip.finishTransmit(); }
size_t  SX1262Radio::chip_packet_length() { return _chip.getPacketLength(); }
int16_t SX1262Radio::chip_read(uint8_t* buf, size_t len) { return _chip.readData(buf, len); }

bool SX1262Radio::chip_rx_header_has_crc() {
	bool has_crc = false;
	// A failed read counts as no CRC, so the packet is dropped rather than trusted
	if (_chip.getLoRaRxHeaderInfo(nullptr, &has_crc) != RADIOLIB_ERR_NONE) return false;
	return has_crc;
}
float   SX1262Radio::chip_packet_rssi() { return _chip.getRSSI(true); }
float   SX1262Radio::chip_packet_snr() { return _chip.getSNR(); }
float   SX1262Radio::current_rssi() { return _chip.getRSSI(false); }
uint32_t SX1262Radio::time_on_air_us(size_t len) { return _chip.getTimeOnAir(len); }

int16_t SX1262Radio::reset_agc() {
	int16_t state = standby();
	if (state == RADIOLIB_ERR_NONE) state = _chip.resetAGC();
	start_receive();
	return state;
}

// PREAMBLE_DETECTED / HEADER_VALID latch in the chip's IRQ status while a frame
// is being received; DIO1 only fires when it ends. This lets transmit wait for a
// reception in progress instead of cutting it off. Each stage has a staleness
// bound so a stray flag cannot hold transmit off forever: a preamble must turn
// into a header within about one symbol-time of airtime, a header into a frame
// within the airtime of the longest frame, both padded by half.
bool SX1262Radio::receiving_packet() {
	if (_transmitting) return false;
	uint32_t irq = _chip.getIrqFlags();
	bool header   = (irq & RADIOLIB_SX126X_IRQ_HEADER_VALID) != 0;
	bool preamble = (irq & RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED) != 0;
	uint32_t now = millis();

	if (!header && _rx_header_seen) {
		// The header flag went away without us clearing it: the frame ended
		_rx_activity_at = 0;
		_rx_header_seen = false;
		return false;
	}
	if (header) {
		if (!_rx_header_seen) { _rx_header_seen = true; _rx_activity_at = now; }
		uint32_t max_ms = (time_on_air_us(MAX_FRAME_LEN) / 1000) * 3 / 2 + 100;
		if (now - _rx_activity_at > max_ms) {
			_chip.clearIrqFlags(RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED | RADIOLIB_SX126X_IRQ_HEADER_VALID);
			_rx_activity_at = 0;
			_rx_header_seen = false;
			return false;
		}
		return true;
	}
	if (preamble) {
		if (_rx_activity_at == 0) _rx_activity_at = now;
		uint32_t pre_ms = (time_on_air_us(1) / 1000) * 3 / 2 + 100;
		if (now - _rx_activity_at > pre_ms) {
			_chip.clearIrqFlags(RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
			_rx_activity_at = 0;
			return false;
		}
		return true;
	}
	_rx_activity_at = 0;
	return false;
}

#endif

// LR2021 driver: sub-GHz and 2.4 GHz LoRa on one chip.
//
// Ported from the radio HAL in openHop_modem (https://github.com/dkmr27/openhop_modem,
// branch radio-hal), MIT licence, by the openHop_modem project and its
// contributors, whose chip details follow MeshCore's CustomLR2021: the IRQ on
// DIO9, a TCXO with a crystal fallback, boosted gain set only in standby. The
// licence notice is in licenses/openhop_modem.txt.

#include "radio/lr2021/LR2021Radio.h"

#if RADIO_USES(RADIO_CHIP_LR2021)

// Limits match RadioLib's checks in LR2021::setFrequency, checkOutputPower,
// setSpreadingFactor and setCodingRate. Only the 2.4 GHz ISM range of the high
// frequency path is listed. Board ceilings come on top.
static const RadioBandInfo LR2021_BANDS[] = {
	{ RadioBand::SubGhz,  150000000UL, 1090000000UL,  -9, 22, 5, 12, 5, 8 },
	{ RadioBand::Band2G4, 2400000000UL, 2500000000UL, -19, 12, 5, 12, 5, 8 },
};

// Longest LoRa frame, for the receive guard's staleness bound
static const size_t MAX_FRAME_LEN = 255;
// PA ramp time used with every power change
static const uint32_t PA_RAMP_US = 48;
// Drive level valid on both PAs, parked at before changing band: the chip checks
// its PA configuration against the current power, so moving from 14+ dBm on the
// sub-GHz PA (2.4 GHz max is 12) or from below -9 dBm on the 2.4 GHz PA (sub-GHz
// min is -9) would otherwise be rejected
static const int8_t PARK_POWER_DBM = -9;
// Level for the RX boosted gain setting: RadioLib takes 0 (off) to 7
static const uint8_t BOOSTED_GAIN_LEVEL = 7;

// RF switch on the chip's own DIOs, from the board. RadioLib keeps pointers to
// these, so they must outlive setRfSwitchTable().
#if defined(RADIO0_RF_SWITCH_PINS)
static uint32_t rf_switch_pins[Module::RFSWITCH_MAX_PINS];
static const uint32_t RF_SWITCH_BOARD_PINS[] = RADIO0_RF_SWITCH_PINS;
static const Module::RfSwitchMode_t RF_SWITCH_MODES[] = RADIO0_RF_SWITCH_MODES;
#endif

LR2021Radio::LR2021Radio(uint8_t index, const RadioPins& pins, SPIClass& spi)
	: Radio(index, pins),
	  _chip(new Module(pins.cs, pins.dio1, pins.reset, pins.busy, spi)) {}

int16_t LR2021Radio::begin(const RadioConfig& c) {
	// Start on sub-GHz at the PA minimum; configure() then moves to `c` with the
	// power clamped for its band
	int16_t state = _chip.begin(434.0f, 125.0f, 9, 7, c.sync_word, PARK_POWER_DBM,
		c.preamble_len, _pins.tcxo_volts);
	if (state == RADIOLIB_ERR_SPI_CMD_FAILED || state == RADIOLIB_ERR_SPI_CMD_INVALID) {
		// Some modules have a crystal rather than a TCXO
		state = _chip.begin(434.0f, 125.0f, 9, 7, c.sync_word, PARK_POWER_DBM, c.preamble_len, 0.0f);
	}
	if (state != RADIOLIB_ERR_NONE) return state;
	_chip_freq_hz = 434000000UL;

#if defined(RADIO0_RF_SWITCH_PINS)
	if (_index == 0) {
		for (size_t i = 0; i < Module::RFSWITCH_MAX_PINS; i++) {
			rf_switch_pins[i] = i < sizeof(RF_SWITCH_BOARD_PINS) / sizeof(RF_SWITCH_BOARD_PINS[0])
				? RF_SWITCH_BOARD_PINS[i] : RADIOLIB_NC;
		}
		_chip.setRfSwitchTable(rf_switch_pins, RF_SWITCH_MODES);
	}
#endif
	_chip.setPacketReceivedAction(irq_handler());

	return configure(c);
}

uint8_t LR2021Radio::bands(const RadioBandInfo*& table) const {
	table = LR2021_BANDS;
	return sizeof(LR2021_BANDS) / sizeof(LR2021_BANDS[0]);
}

bool LR2021Radio::accepts_bandwidth(RadioBand band, uint32_t bw_hz) const {
	// Same bucket test as LR2021::setBandwidth(), which takes the same set on
	// either path
	(void)band;
	float bw = bw_hz / 1000.0f;
	if (bw < 0.0f || bw > 1010.0f) return false;
	switch ((int)(bw / 2 + 0.01f)) {
	case 15: case 20: case 31: case 41: case 50: case 62:
	case 101: case 125: case 203: case 250: case 406: case 500:
		return true;
	default:
		return false;
	}
}

// ─── Configuration ──────────────────────────────────────────
int16_t LR2021Radio::standby() { return _chip.standby(); }
int16_t LR2021Radio::sleep() { return _chip.sleep(true, 0); }  // keeps the configuration

#if defined(RRPTR_TEST_NOCRC)
int16_t LR2021Radio::test_transmit_no_crc(const uint8_t* data, size_t len) {
	int16_t state = standby();
	if (state == RADIOLIB_ERR_NONE) state = _chip.setCRC(0);
	if (state == RADIOLIB_ERR_NONE) state = _chip.transmit(data, len);
	_chip.setCRC(2);
	start_receive();
	return state;
}
#endif

int16_t LR2021Radio::chip_set_frequency(uint32_t freq_hz) {
	bool band_change = radio_band_for(freq_hz) != radio_band_for(_chip_freq_hz);
	if (band_change) {
		// The caller applies the clamped power for the new band afterwards
		int16_t parked = _chip.setOutputPower(PARK_POWER_DBM, PA_RAMP_US);
		if (parked != RADIOLIB_ERR_NONE) return parked;
	}
	int16_t state = _chip.setFrequency(freq_hz / 1e6f);
	if (state != RADIOLIB_ERR_NONE) return state;
	_chip_freq_hz = freq_hz;
	// The gain mode is kept per receive path; set it for the new one
	if (band_change && _config.rx_boosted_gain) state = _chip.setRxBoostedGainMode(BOOSTED_GAIN_LEVEL);
	return state;
}

int16_t LR2021Radio::chip_set_output_power(int8_t power_dbm) {
	return _chip.setOutputPower(power_dbm, PA_RAMP_US);
}

// Called in standby, which the chip needs for the gain mode
int16_t LR2021Radio::chip_set_modem(const RadioConfig& c) {
	int16_t state;
	if ((state = _chip.setBandwidth(c.bandwidth_hz / 1e3f)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setSpreadingFactor(c.sf)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setCodingRate(c.cr)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setSyncWord(c.sync_word)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setPreambleLength(c.preamble_len)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.explicitHeader()) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setRxBoostedGainMode(c.rx_boosted_gain ? BOOSTED_GAIN_LEVEL : 0)) != RADIOLIB_ERR_NONE) return state;
	return _chip.setCRC(2);
}

// ─── Receive / transmit ─────────────────────────────────────
int16_t LR2021Radio::chip_start_receive() {
	_rx_activity_at = 0;
	_rx_header_seen = false;
	// Latch PREAMBLE_DETECTED too, for receiving_packet()
	return _chip.startReceive(RADIOLIB_LR2021_RX_TIMEOUT_INF,
		RADIOLIB_IRQ_RX_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED),
		RADIOLIB_IRQ_RX_DEFAULT_MASK, 0);
}

int16_t LR2021Radio::chip_start_transmit(const uint8_t* data, size_t len) {
	return _chip.startTransmit(data, len);
}

// Two symbols, and RadioLib's per-SF thresholds: the LR2021's detection scale
// differs from the SX126x's. The scan exits to standby. A failed read counts as
// activity, so we do not talk over someone.
int16_t LR2021Radio::chip_start_scan() {
	ChannelScanConfig_t cfg = {};
	cfg.cad.symNum   = 2;
	cfg.cad.detPeak  = RADIOLIB_LR2021_CAD_PARAM_DEFAULT;
	cfg.cad.detMin   = RADIOLIB_LR2021_CAD_PARAM_DEFAULT;
	cfg.cad.exitMode = RADIOLIB_LR2021_CAD_EXIT_MODE_FALLBACK;
	cfg.cad.timeout  = 0;
	cfg.cad.irqFlags = RADIOLIB_IRQ_CAD_DEFAULT_FLAGS;
	cfg.cad.irqMask  = RADIOLIB_IRQ_CAD_DEFAULT_MASK;
	return _chip.startChannelScan(cfg);
}
bool LR2021Radio::chip_scan_found_activity() { return _chip.getChannelScanResult() != RADIOLIB_CHANNEL_FREE; }

int16_t LR2021Radio::chip_finish_transmit() { return _chip.finishTransmit(); }
size_t  LR2021Radio::chip_packet_length() { return _chip.getPacketLength(); }
int16_t LR2021Radio::chip_read(uint8_t* buf, size_t len) { return _chip.readData(buf, len); }

bool LR2021Radio::chip_rx_header_has_crc() {
	bool has_crc = false;
	// A failed read counts as no CRC, so the packet is dropped rather than trusted
	if (_chip.getLoRaRxHeaderInfo(nullptr, &has_crc) != RADIOLIB_ERR_NONE) return false;
	return has_crc;
}

// readData() leaves the chip in continuous receive, as on the SX1262, so there
// is nothing to restart; restarting would drop a frame that has already begun
void LR2021Radio::chip_continue_receive() {
	_rx_activity_at = 0;
	_rx_header_seen = false;
}

float    LR2021Radio::chip_packet_rssi() { return _chip.getRSSI(true); }
float    LR2021Radio::chip_packet_snr() { return _chip.getSNR(); }
// skipReceive: an instantaneous reading must not restart receive through standby
float    LR2021Radio::current_rssi() { return _chip.getRSSI(false, true); }
uint32_t LR2021Radio::time_on_air_us(size_t len) { return _chip.getTimeOnAir(len); }

int16_t LR2021Radio::reset_agc() { return RADIOLIB_ERR_UNSUPPORTED; }

// As SX1262Radio::receiving_packet(), on the LR2021's IRQ bits. A header CRC
// error ends the reception straight away.
bool LR2021Radio::receiving_packet() {
	if (_transmitting) return false;
	uint32_t irq = _chip.getIrqFlags();
	bool header    = (irq & RADIOLIB_LR2021_IRQ_LORA_HEADER_VALID) != 0;
	bool preamble  = (irq & RADIOLIB_LR2021_IRQ_PREAMBLE_DETECTED) != 0;
	bool hdr_error = (irq & RADIOLIB_LR2021_IRQ_LORA_HDR_CRC_ERROR) != 0;
	uint32_t now = millis();

	if (hdr_error || (!header && _rx_header_seen)) {
		_rx_activity_at = 0;
		_rx_header_seen = false;
		return false;
	}
	if (header) {
		if (!_rx_header_seen) { _rx_header_seen = true; _rx_activity_at = now; }
		uint32_t max_ms = (time_on_air_us(MAX_FRAME_LEN) / 1000) * 3 / 2 + 100;
		if (now - _rx_activity_at > max_ms) {
			_chip.clearIrqFlags(RADIOLIB_LR2021_IRQ_PREAMBLE_DETECTED | RADIOLIB_LR2021_IRQ_LORA_HEADER_VALID);
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
			_chip.clearIrqFlags(RADIOLIB_LR2021_IRQ_PREAMBLE_DETECTED);
			_rx_activity_at = 0;
			return false;
		}
		return true;
	}
	_rx_activity_at = 0;
	return false;
}

#endif

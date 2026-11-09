// LR1121 driver: sub-GHz and 2.4 GHz LoRa on one chip.
//
// Ported from the radio HAL in openHop_modem (https://github.com/dkmr27/openhop_modem,
// branch radio-hal), MIT licence, by the openHop_modem project and its
// contributors, whose chip workarounds follow MeshCore's CustomLR1121: the DC-DC
// regulator, and standby after a header error. The licence notice is in
// licenses/openhop_modem.txt.

#include "radio/lr11x0/LR1121Radio.h"

#if RADIO_USES(RADIO_CHIP_LR1121)

// Limits match RadioLib's checks in LR1120::setFrequency / checkOutputPower and
// LR11x0::setSpreadingFactor / setCodingRate. The sub-GHz minimum is the low
// power PA's. Only the 2.4 GHz ISM range is listed. Board ceilings come on top.
static const RadioBandInfo LR1121_BANDS[] = {
	{ RadioBand::SubGhz,  150000000UL, 960000000UL,  -17, 22, 5, 12, 5, 8 },
	{ RadioBand::Band2G4, 2400000000UL, 2500000000UL, -18, 13, 5, 12, 5, 8 },
};

// Longest LoRa frame, for the receive guard's staleness bound
static const size_t MAX_FRAME_LEN = 255;
// PA ramp time used with every power change
static const uint32_t PA_RAMP_US = 48;
// Drive level every PA accepts, used at start-up
static const int8_t START_POWER_DBM = -9;
// CAD detection peak per SF (SF5-SF12), measured on the bench in openHop_modem
// on both bands: RadioLib's defaults gave 5-18 % false alarms on an idle channel
// with two-symbol scans, these 0-2 % with full detection up to SF11
static const uint8_t CAD_DET_PEAK[8] = { 56, 56, 56, 58, 60, 62, 64, 67 };

// RF switch on the chip's own DIOs, from the board. RadioLib keeps pointers to
// these, so they must outlive setRfSwitchTable().
#if defined(RADIO0_RF_SWITCH_PINS)
static uint32_t rf_switch_pins[Module::RFSWITCH_MAX_PINS];
static const uint32_t RF_SWITCH_BOARD_PINS[] = RADIO0_RF_SWITCH_PINS;
static const Module::RfSwitchMode_t RF_SWITCH_MODES[] = RADIO0_RF_SWITCH_MODES;
#endif

LR1121Radio::LR1121Radio(uint8_t index, const RadioPins& pins, SPIClass& spi)
	: Radio(index, pins),
	  _chip(new Module(pins.cs, pins.dio1, pins.reset, pins.busy, spi)) {}

// Brings the chip up from reset on sub-GHz, without the modem settings
int16_t LR1121Radio::chip_init(uint8_t sync_word, uint16_t preamble_len) {
	int16_t state = _chip.begin(434.0f, 125.0f, 9, 7, sync_word, START_POWER_DBM,
		preamble_len, _pins.tcxo_volts);
	if (state != RADIOLIB_ERR_NONE) return state;
	_chip_freq_hz = 434000000UL;
	// RadioLib's begin() selects the LDO regulator; the boards are designed for DC-DC
	state = _chip.setRegulatorDCDC();
	if (state != RADIOLIB_ERR_NONE) return state;
#if defined(RADIO0_RF_SWITCH_PINS)
	if (_index == 0) {
		for (size_t i = 0; i < Module::RFSWITCH_MAX_PINS; i++) {
			rf_switch_pins[i] = i < sizeof(RF_SWITCH_BOARD_PINS) / sizeof(RF_SWITCH_BOARD_PINS[0])
				? RF_SWITCH_BOARD_PINS[i] : RADIOLIB_NC;
		}
		_chip.setRfSwitchTable(rf_switch_pins, RF_SWITCH_MODES);
	}
#endif
	// The chip raises every enabled IRQ on its DIO9, wired to the board's DIO1 pin
	_chip.setPacketReceivedAction(irq_handler());
	return RADIOLIB_ERR_NONE;
}

int16_t LR1121Radio::begin(const RadioConfig& c) {
	int16_t state = chip_init(c.sync_word, c.preamble_len);
	if (state != RADIOLIB_ERR_NONE) return state;
	return configure(c);
}

uint8_t LR1121Radio::bands(const RadioBandInfo*& table) const {
	table = LR1121_BANDS;
	return sizeof(LR1121_BANDS) / sizeof(LR1121_BANDS[0]);
}

// The SX128x-compatible bandwidths, 2.4 GHz only. RadioLib's setBandwidth()
// takes these with its `high` flag set, and the others without it.
static bool is_sx128x_bandwidth(uint32_t bw_hz) {
	return bw_hz == 203125 || bw_hz == 406250 || bw_hz == 812500;
}

// 62.5 to 500 kHz on either band, as LR11x0::setBandwidth() buckets them, and on
// 2.4 GHz the SX128x-compatible ones too
bool LR1121Radio::accepts_bandwidth(RadioBand band, uint32_t bw_hz) const {
	if (is_sx128x_bandwidth(bw_hz)) return band == RadioBand::Band2G4;
	float bw = bw_hz / 1000.0f;
	if (bw < 0.0f || bw > 510.0f) return false;
	switch ((uint8_t)(bw / 2 + 0.01f)) {
	case 31: case 62: case 125: case 250:
		return true;
	default:
		return false;
	}
}

// ─── Configuration ──────────────────────────────────────────
int16_t LR1121Radio::standby() { return _chip.standby(); }
int16_t LR1121Radio::sleep() { return _chip.sleep(true, 0); }  // keeps the configuration

#if defined(RRPTR_TEST_NOCRC)
int16_t LR1121Radio::test_transmit_no_crc(const uint8_t* data, size_t len) {
	int16_t state = standby();
	if (state == RADIOLIB_ERR_NONE) state = _chip.setCRC(0);
	if (state == RADIOLIB_ERR_NONE) state = _chip.transmit(data, len);
	_chip.setCRC(2);
	start_receive();
	return state;
}
#endif

int16_t LR1121Radio::chip_set_frequency(uint32_t freq_hz) {
	int16_t state;
	if (radio_band_for(freq_hz) == RadioBand::Band2G4) {
		// Image calibration is for sub-GHz only, and RadioLib's band arithmetic
		// for it overflows above about 1 GHz: skip it
		state = _chip.setFrequency(freq_hz / 1e6f, true);
	} else {
		if (radio_band_for(_chip_freq_hz) == RadioBand::Band2G4) {
			// After 2.4 GHz use every sub-GHz image calibration fails until the
			// chip is reset, so start it again. That loses the modem settings,
			// which are put back; the caller applies the power after this.
			state = chip_init(_config.sync_word, _config.preamble_len);
			if (state == RADIOLIB_ERR_NONE) state = chip_set_modem(_config);
			if (state != RADIOLIB_ERR_NONE) return state;
		}
		state = _chip.setFrequency(freq_hz / 1e6f);
	}
	if (state == RADIOLIB_ERR_NONE) _chip_freq_hz = freq_hz;
	return state;
}

int16_t LR1121Radio::chip_set_output_power(int8_t power_dbm) {
	// RadioLib picks the PA from the frequency and power
	return _chip.setOutputPower(power_dbm, false, PA_RAMP_US);
}

int16_t LR1121Radio::chip_set_modem(const RadioConfig& c) {
	int16_t state;
	if ((state = _chip.setBandwidth(c.bandwidth_hz / 1e3f, is_sx128x_bandwidth(c.bandwidth_hz))) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setSpreadingFactor(c.sf)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setCodingRate(c.cr)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setSyncWord(c.sync_word)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setPreambleLength(c.preamble_len)) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.explicitHeader()) != RADIOLIB_ERR_NONE) return state;
	if ((state = _chip.setRxBoostedGainMode(c.rx_boosted_gain)) != RADIOLIB_ERR_NONE) return state;
	return _chip.setCRC(2);
}

// ─── Receive / transmit ─────────────────────────────────────
int16_t LR1121Radio::chip_start_receive() {
	_restart_rx = false;
	_rx_activity_at = 0;
	_rx_header_seen = false;
	// Latch PREAMBLE_DETECTED too, for receiving_packet()
	return _chip.startReceive(RADIOLIB_LR11X0_RX_TIMEOUT_INF,
		RADIOLIB_IRQ_RX_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED),
		RADIOLIB_IRQ_RX_DEFAULT_MASK, 0);
}

int16_t LR1121Radio::chip_start_transmit(const uint8_t* data, size_t len) {
	return _chip.startTransmit(data, len);
}

// Two symbols, with the bench-tuned detection peak for the SF. The scan exits to
// standby. A failed read counts as activity, so we do not talk over someone.
int16_t LR1121Radio::chip_start_scan() {
	uint8_t sf = _config.sf < 5 ? 5 : _config.sf > 12 ? 12 : _config.sf;
	ChannelScanConfig_t cfg = {};
	cfg.cad.symNum   = 2;
	cfg.cad.detPeak  = CAD_DET_PEAK[sf - 5];
	cfg.cad.detMin   = RADIOLIB_LR11X0_CAD_PARAM_DEFAULT;
	cfg.cad.exitMode = RADIOLIB_LR11X0_CAD_EXIT_MODE_STBY_RC;
	cfg.cad.timeout  = 0;
	cfg.cad.irqFlags = RADIOLIB_IRQ_CAD_DEFAULT_FLAGS;
	cfg.cad.irqMask  = RADIOLIB_IRQ_CAD_DEFAULT_MASK;
	return _chip.startChannelScan(cfg);
}
bool LR1121Radio::chip_scan_found_activity() { return _chip.getChannelScanResult() != RADIOLIB_CHANNEL_FREE; }

int16_t LR1121Radio::chip_finish_transmit() { return _chip.finishTransmit(); }

size_t LR1121Radio::chip_packet_length() {
	size_t len = _chip.getPacketLength();
	if (len == 0 && (_chip.getIrqFlags() & RADIOLIB_LR11X0_IRQ_HEADER_ERR)) {
		// A corrupted header can leave the chip shifting later packets. Standby
		// returns it to a known state, and receive is restarted after the read.
		_chip.standby();
		_restart_rx = true;
	}
	return len;
}

int16_t LR1121Radio::chip_read(uint8_t* buf, size_t len) { return _chip.readData(buf, len); }

bool LR1121Radio::chip_rx_header_has_crc() {
	bool has_crc = false;
	// A failed read counts as no CRC, so the packet is dropped rather than trusted
	if (_chip.getLoRaRxHeaderInfo(nullptr, &has_crc) != RADIOLIB_ERR_NONE) return false;
	return has_crc;
}

// readData() leaves the chip in continuous receive, as on the SX1262, so there
// is nothing to restart unless a header error put it in standby
void LR1121Radio::chip_continue_receive() {
	if (_restart_rx) { start_receive(); return; }
	_rx_activity_at = 0;
	_rx_header_seen = false;
}

float    LR1121Radio::chip_packet_rssi() { return _chip.getRSSI(true); }
float    LR1121Radio::chip_packet_snr() { return _chip.getSNR(); }
// skipReceive: an instantaneous reading must not restart receive through standby
float    LR1121Radio::current_rssi() { return _chip.getRSSI(false, true); }
uint32_t LR1121Radio::time_on_air_us(size_t len) { return _chip.getTimeOnAir(len); }

int16_t LR1121Radio::reset_agc() { return RADIOLIB_ERR_UNSUPPORTED; }

// As SX1262Radio::receiving_packet(), on the LR11x0's IRQ bits. A header error
// ends the reception straight away.
bool LR1121Radio::receiving_packet() {
	if (_transmitting) return false;
	uint32_t irq = _chip.getIrqFlags();
	bool header    = (irq & RADIOLIB_LR11X0_IRQ_SYNC_WORD_HEADER_VALID) != 0;
	bool preamble  = (irq & RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED) != 0;
	bool hdr_error = (irq & RADIOLIB_LR11X0_IRQ_HEADER_ERR) != 0;
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
			_chip.clearIrqFlags(RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED | RADIOLIB_LR11X0_IRQ_SYNC_WORD_HEADER_VALID);
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
			_chip.clearIrqFlags(RADIOLIB_LR11X0_IRQ_PREAMBLE_DETECTED);
			_rx_activity_at = 0;
			return false;
		}
		return true;
	}
	_rx_activity_at = 0;
	return false;
}

#endif

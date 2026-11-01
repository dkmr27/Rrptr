// Radio: one LoRa radio, independent of the chip.
//
// One instance per fitted radio, created by radio_create() from the RADIOn_*
// wiring in board_config.h. Chip drivers (SX1262Radio, later LR1121, SX1280,
// LR2021) implement the protected chip hooks; band-aware power limits, config
// validation and the receive/transmit state are shared here.
//
// The interface follows the radio HAL in openHop_modem (dad650f), reshaped
// from a single global into instances so two radios can run side by side.
// Return codes are RadioLib's (RADIOLIB_ERR_NONE on success).

#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>

#include "board/RadioPins.h"
#include "radio/RadioBand.h"

// read_packet(): the frame's header said it carries no CRC, so its payload was
// never checked. Outside RadioLib's error code range.
#define RADIO_ERR_NO_CRC (-1100)

class Radio {
public:
	enum Event : uint8_t {
		EVENT_NONE,
		EVENT_RX_DONE,      // a packet (or a failed one) is waiting: call read_packet()
		EVENT_TX_DONE,      // transmit finished, radio is back in receive
	};

	Radio(uint8_t index, const RadioPins& pins) : _index(index), _pins(pins) {}
	virtual ~Radio() {}

	uint8_t index() const { return _index; }
	virtual const char* chip_name() const = 0;

	// Initialises the chip and applies `config`. The radio is left in standby.
	virtual int16_t begin(const RadioConfig& config) = 0;

	// ─── Capabilities ───────────────────────────────────────
	// The chip's band table (what it can do, before board limits)
	virtual uint8_t bands(const RadioBandInfo*& table) const = 0;
	// Effective power ceiling at a frequency, or RADIO_POWER_NONE
	int8_t max_output_power(uint32_t freq_hz) const;
	// True if every field of `config` is valid for this chip and board.
	// Power above the ceiling is allowed (it is clamped).
	bool config_supported(const RadioConfig& config) const;

	// ─── Configuration ──────────────────────────────────────
	// Validates and applies a whole config; on failure nothing is changed
	int16_t configure(const RadioConfig& config);
	const RadioConfig& config() const { return _config; }
	int16_t set_frequency(uint32_t freq_hz);
	// Clamped to the band's ceiling; applied_power_dbm() tells what was set
	int16_t set_output_power(int8_t power_dbm);
	int8_t  applied_power_dbm() const { return _applied_power_dbm; }

	// ─── Receive / transmit ─────────────────────────────────
	int16_t start_receive();
	int16_t start_transmit(const uint8_t* data, size_t len);
	bool    transmitting() const { return _transmitting; }

	// Call from the main loop: handles the radio interrupt and reports what
	// happened. After EVENT_TX_DONE the radio is already receiving again.
	Event   poll();

	// Reads the waiting packet into `buf` and restarts receive. Returns the
	// packet length, or a negative error: RadioLib's (CRC, header) or
	// RADIO_ERR_NO_CRC. On error the contents of `buf` must not be used.
	int16_t read_packet(uint8_t* buf, size_t max_len);
	float   packet_rssi() const { return _rssi; }
	float   packet_snr() const { return _snr; }

	// Instantaneous RSSI while receiving (noise floor)
	virtual float    current_rssi() = 0;
	virtual uint32_t time_on_air_us(size_t len) = 0;
	// True while a frame is being received (preamble or header seen)
	virtual bool     receiving_packet() = 0;
	// Warm-sleep AGC recalibration; the radio is put back in receive
	virtual int16_t  reset_agc() = 0;
	virtual int16_t  standby() = 0;

protected:
	// ─── Chip hooks ─────────────────────────────────────────
	virtual bool    accepts_bandwidth(RadioBand band, uint32_t bw_hz) const = 0;
	virtual bool    accepts_preamble(uint16_t len) const = 0;
	virtual int16_t chip_set_frequency(uint32_t freq_hz) = 0;
	virtual int16_t chip_set_output_power(int8_t power_dbm) = 0;
	virtual int16_t chip_set_modem(const RadioConfig& config) = 0;   // bw, sf, cr, sync, preamble
	virtual int16_t chip_start_receive() = 0;
	virtual int16_t chip_start_transmit(const uint8_t* data, size_t len) = 0;
	virtual int16_t chip_finish_transmit() = 0;
	virtual size_t  chip_packet_length() = 0;
	virtual int16_t chip_read(uint8_t* buf, size_t len) = 0;
	// Whether the header of the frame just read declared a payload CRC
	virtual bool    chip_rx_header_has_crc() = 0;
	virtual float   chip_packet_rssi() = 0;
	virtual float   chip_packet_snr() = 0;

	// Hooks DIO1 up to this instance's interrupt flag
	void (*irq_handler() const)(void);
	int8_t board_ceiling(RadioBand band) const;
	const RadioBandInfo* band_info(RadioBand band) const;
	int16_t apply_output_power();

	const uint8_t     _index;
	const RadioPins&  _pins;
	RadioConfig       _config = {};
	int8_t            _requested_power_dbm = 0;
	int8_t            _applied_power_dbm = 0;
	bool              _transmitting = false;
	float             _rssi = 0;
	float             _snr = 0;
};

// Creates the driver for radio `index` on `spi`; nullptr if its chip is not
// supported in this build.
Radio* radio_create(uint8_t index, SPIClass& spi);

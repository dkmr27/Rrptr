#pragma once

#include "radio/Radio.h"

class LR1121Radio : public Radio {
public:
	LR1121Radio(uint8_t index, const RadioPins& pins, SPIClass& spi);

	const char* chip_name() const override { return "LR1121"; }
	int16_t begin(const RadioConfig& config) override;
	uint8_t bands(const RadioBandInfo*& table) const override;

	float    current_rssi() override;
	uint32_t time_on_air_us(size_t len) override;
	bool     receiving_packet() override;
	int16_t  reset_agc() override;
	int16_t  standby() override;
	int16_t  sleep() override;
#if defined(RRPTR_TEST_NOCRC)
	int16_t  test_transmit_no_crc(const uint8_t* data, size_t len) override;
#endif

protected:
	bool    accepts_bandwidth(RadioBand band, uint32_t bw_hz) const override;
	bool    accepts_preamble(uint16_t len) const override { (void)len; return true; }
	int16_t chip_set_frequency(uint32_t freq_hz) override;
	int16_t chip_set_output_power(int8_t power_dbm) override;
	int16_t chip_set_modem(const RadioConfig& config) override;
	int16_t chip_start_receive() override;
	int16_t chip_start_transmit(const uint8_t* data, size_t len) override;
	int16_t chip_start_scan() override;
	bool    chip_scan_found_activity() override;
	int16_t chip_finish_transmit() override;
	size_t  chip_packet_length() override;
	int16_t chip_read(uint8_t* buf, size_t len) override;
	bool    chip_rx_header_has_crc() override;
	void    chip_continue_receive() override;
	float   chip_packet_rssi() override;
	float   chip_packet_snr() override;

private:
	int16_t chip_init(uint8_t sync_word, uint16_t preamble_len);

	LR1121   _chip;
	uint32_t _chip_freq_hz = 0;     // frequency the chip is on, for band changes
	bool     _restart_rx = false;   // a header error left the chip in standby
	uint32_t _rx_activity_at = 0;
	bool     _rx_header_seen = false;
};

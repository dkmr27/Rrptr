// LoRaInterface: a microReticulum interface on one Radio, using the RNode
// on-air format so Rrptr talks to existing RNode-based Reticulum nodes.
//
// On-air format (as in the MIT-licensed RNode firmware up to fe5e6aa, 2022):
//   - Every LoRa frame starts with a 1-byte header: a random high nibble used
//     as the sequence number, and bit 0 set when the packet is split.
//   - A Reticulum packet of up to 254 bytes goes in one frame.
//   - A longer packet (up to 508 bytes) is split over two frames, each carrying
//     the same header byte: 254 bytes in the first, the rest in the second.
//   - LoRa explicit header, CRC on, private sync word.

#pragma once

#include <deque>

#include <microReticulum/Interface.h>
#include <microReticulum/Bytes.h>

#include "radio/Radio.h"

class LoRaInterface : public RNS::InterfaceImpl {
public:
	static const size_t HW_MTU = 508;           // largest Reticulum packet
	static const size_t FRAME_PAYLOAD = 254;    // packet bytes per LoRa frame
	static const size_t TX_QUEUE_MAX = 8;

	LoRaInterface(Radio& radio, const char* name);

	// Packet and drop counters, for status output
	struct Stats {
		uint32_t rx_packets, rx_frames, rx_errors, rx_no_crc, rx_orphans;
		uint32_t tx_packets, tx_dropped, tx_airtime_ms;
	};
	const Stats& stats() const { return _stats; }

	// Online when the radio is configured and receiving; offline drops anything
	// queued. Call after every radio settings change (updates the bitrate).
	void set_online(bool online);
	bool online() const { return _online; }
	uint32_t queued() const { return _tx_queue.size(); }

protected:
	bool start() override;
	bool send_outgoing(const RNS::Bytes& data) override;
	void loop() override;

private:
	void receive_frame();
	void deliver(const uint8_t* data, size_t len, float rssi, float snr);
	void start_next_packet();
	void transmit_frame(const uint8_t* data, size_t len);
	void transmit_done();
	void update_bitrate();

	Radio& _radio;
	Stats  _stats = {};

	// Receive: first half of a split packet while waiting for the second
	uint8_t _rx_buf[HW_MTU];
	size_t  _rx_len = 0;
	int16_t _rx_seq = -1;           // sequence nibble of the waiting half, -1 = none
	float   _rx_rssi = 0, _rx_snr = 0;

	// Transmit: queued packets, and the one going out now
	std::deque<RNS::Bytes> _tx_queue;
	RNS::Bytes _tx_packet;
	size_t  _tx_sent = 0;           // packet bytes already sent
	uint8_t _tx_header = 0;
	bool    _tx_active = false;
	uint8_t _tx_frame[1 + FRAME_PAYLOAD];
};

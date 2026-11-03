#include "lora/LoRaInterface.h"

#include <microReticulum/Cryptography/Random.h>
#include "log/Log.h"

#include "board/Board.h"

static const uint8_t HEADER_SPLIT = 0x01;
static const uint8_t HEADER_SEQ   = 0xF0;

LoRaInterface::LoRaInterface(Radio& radio, const char* name)
	: RNS::InterfaceImpl(name), _radio(radio) {
	_HW_MTU = HW_MTU;
	_IN = true;
	_OUT = true;
}

bool LoRaInterface::start() {
	update_bitrate();
	return true;
}

void LoRaInterface::set_online(bool online) {
	if (!online) {
		// Nothing queued or half-received survives a radio change
		_tx_queue.clear();
		_tx_active = false;
		_rx_seq = -1;
		board.tx_led(false);
	}
	update_bitrate();
	_online = online;
}

// LoRa bitrate from the modem settings, so microReticulum's announce rate
// limits and timeouts match the channel
void LoRaInterface::update_bitrate() {
	const RadioConfig& c = _radio.config();
	float symbol_rate = (float)c.bandwidth_hz / (float)(1UL << c.sf);
	_bitrate = (uint32_t)(c.sf * symbol_rate * 4.0f / c.cr);
}

// ─── Receive ────────────────────────────────────────────────
void LoRaInterface::loop() {
	switch (_radio.poll()) {
	case Radio::EVENT_RX_DONE: receive_frame(); break;
	case Radio::EVENT_TX_DONE: transmit_done(); break;
	default: break;
	}
	if (_online && !_tx_active) start_next_packet();
}

void LoRaInterface::receive_frame() {
	uint8_t frame[1 + FRAME_PAYLOAD + 1];
	int16_t len = _radio.read_packet(frame, sizeof(frame));
	if (len < 0) {
		if (len == RADIO_ERR_NO_CRC) {
			_stats.rx_no_crc++;
#if defined(RRPTR_TEST_NOCRC)
			R_DEBUG("LoRa: dropped frame without CRC, RSSI %.1f, SNR %.2f", _radio.packet_rssi(), _radio.packet_snr());
#endif
		} else {
			_stats.rx_errors++;
			R_DEBUG("LoRa: rx error %d, RSSI %.1f, SNR %.2f", len, _radio.packet_rssi(), _radio.packet_snr());
		}
		return;
	}
	_stats.rx_frames++;
	if (len < 2) return;  // header only

	uint8_t header = frame[0];
	int16_t seq = header & HEADER_SEQ;
	const uint8_t* payload = frame + 1;
	size_t payload_len = len - 1;
	float rssi = _radio.packet_rssi();
	float snr = _radio.packet_snr();

	if (!(header & HEADER_SPLIT)) {
		// Whole packet in one frame; a waiting first half can no longer complete
		if (_rx_seq >= 0) _stats.rx_orphans++;
		_rx_seq = -1;
		deliver(payload, payload_len, rssi, snr);
		return;
	}

	if (_rx_seq == seq && _rx_len + payload_len <= HW_MTU) {
		// Second half: join the two and report the average signal
		memcpy(_rx_buf + _rx_len, payload, payload_len);
		size_t total = _rx_len + payload_len;
		_rx_seq = -1;
		deliver(_rx_buf, total, (_rx_rssi + rssi) / 2, (_rx_snr + snr) / 2);
		return;
	}

	// First half (or a new split packet replacing an unfinished one)
	if (_rx_seq >= 0) _stats.rx_orphans++;
	memcpy(_rx_buf, payload, payload_len);
	_rx_len = payload_len;
	_rx_seq = seq;
	_rx_rssi = rssi;
	_rx_snr = snr;
}

void LoRaInterface::deliver(const uint8_t* data, size_t len, float rssi, float snr) {
	_stats.rx_packets++;
	// Reticulum header: flags, hops, 1 or 2 addresses (bit 6 of flags), context
	size_t context_at = (data[0] & 0x40) ? 34 : 18;
	R_DEBUG("LoRa: rx %u bytes, flags 0x%02x, context 0x%02x, RSSI %.1f, SNR %.2f", (unsigned)len,
		data[0], len > context_at ? data[context_at] : 0, rssi, snr);
	std::shared_ptr<RNS::InterfaceImpl> self = shared_from_this();
	RNS::Interface iface(self);
	iface.r_stat_rssi(rssi);
	iface.r_stat_snr(snr);
	iface.handle_incoming(RNS::Bytes(data, len));
}

// ─── Transmit ───────────────────────────────────────────────
bool LoRaInterface::send_outgoing(const RNS::Bytes& data) {
	if (!_online) return false;
	if (data.size() == 0 || data.size() > HW_MTU || _tx_queue.size() >= TX_QUEUE_MAX) {
		_stats.tx_dropped++;
		R_WARNING("LoRa: dropped outgoing packet (%u bytes, %u queued)", data.size(), _tx_queue.size());
		return false;
	}
	_tx_queue.push_back(data);
	return true;
}

void LoRaInterface::start_next_packet() {
	if (_tx_queue.empty() || _radio.transmitting()) return;
	// Listen before talk: let a frame being received finish first
	if (_radio.receiving_packet()) return;

	_tx_packet = _tx_queue.front();
	_tx_queue.pop_front();
	_tx_sent = 0;
	_tx_header = RNS::Cryptography::random(1).data()[0] & HEADER_SEQ;
	if (_tx_packet.size() > FRAME_PAYLOAD) _tx_header |= HEADER_SPLIT;
	_tx_active = true;
	board.tx_led(true);

	size_t len = std::min(_tx_packet.size(), FRAME_PAYLOAD);
	transmit_frame(_tx_packet.data(), len);
}

void LoRaInterface::transmit_frame(const uint8_t* data, size_t len) {
	_tx_frame[0] = _tx_header;
	memcpy(_tx_frame + 1, data, len);
	_tx_sent += len;
	_stats.tx_airtime_ms += _radio.time_on_air_us(1 + len) / 1000;
	int16_t state = _radio.start_transmit(_tx_frame, 1 + len);
	if (state != RADIOLIB_ERR_NONE) {
		R_ERROR("LoRa: transmit failed (%d)", state);
		_stats.tx_dropped++;
		_tx_active = false;
		board.tx_led(false);
	}
}

void LoRaInterface::transmit_done() {
	if (!_tx_active) return;
	if (_tx_sent < _tx_packet.size()) {
		// Second half of a split packet, straight after the first
		transmit_frame(_tx_packet.data() + _tx_sent, _tx_packet.size() - _tx_sent);
		return;
	}
	_tx_active = false;
	board.tx_led(false);
	_stats.tx_packets++;
	handle_outgoing(_tx_packet);
}

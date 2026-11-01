// Rrptr: Reticulum transport node for LoRa.
//
// Radio layer stage: brings up the board and radio 0 with fixed test settings
// and listens. Every packet heard is printed with its RSSI, SNR and length.
// Serial keys (temporary, until the CLI): t = transmit a test packet,
// n = noise floor, a = AGC reset, s = status.

#include <Arduino.h>
#if defined(RRPTR_MCU_NRF52)
#include <Adafruit_TinyUSB.h>
#endif

#include "board/Board.h"
#include "radio/Radio.h"

// Fixed test settings until the settings store exists
static const RadioConfig TEST_CONFIG = {
	925875000,      // Hz
	250000,         // bandwidth Hz
	9,              // SF
	5,              // CR 4/5
	10,             // dBm
	0x12,           // private sync word
	18,             // preamble symbols
};

static Radio* radio = nullptr;
static int16_t radio_error = RADIOLIB_ERR_NONE;
static uint32_t tx_count = 0;

static void report() {
	Serial.printf("Rrptr %s (%s, %s) on %s, reset: %s, battery %u mV\r\n",
		RRPTR_VERSION, RRPTR_GIT_HASH, RRPTR_BUILD_DATE, board.name(),
		board.reset_reason(), board.battery_mv());
	if (radio) {
		const RadioConfig& c = radio->config();
		Serial.printf("radio0 %s: %.3f MHz, %.1f kHz, SF%u, CR4/%u, %d dBm (max %d)\r\n",
			radio->chip_name(), c.freq_hz / 1e6f, c.bandwidth_hz / 1e3f, c.sf, c.cr,
			radio->applied_power_dbm(), radio->max_output_power(c.freq_hz));
	}
}

static void print_packet(const uint8_t* buf, int16_t len) {
	Serial.printf("rx %d bytes, RSSI %.1f dBm, SNR %.2f dB:", len,
		radio->packet_rssi(), radio->packet_snr());
	for (int16_t i = 0; i < len && i < 32; i++) Serial.printf(" %02x", buf[i]);
	if (len > 32) Serial.print(" ...");
	Serial.print("\r\n");
}

static void transmit_test() {
	if (radio->transmitting()) { Serial.print("tx busy\r\n"); return; }
	char msg[48];
	int len = snprintf(msg, sizeof(msg), "Rrptr test %lu", (unsigned long)++tx_count);
	board.led(true);
	uint32_t toa_ms = radio->time_on_air_us(len) / 1000;
	int16_t state = radio->start_transmit((const uint8_t*)msg, len);
	if (state != RADIOLIB_ERR_NONE) {
		board.led(false);
		Serial.printf("tx failed: %d\r\n", state);
		return;
	}
	Serial.printf("tx %d bytes \"%s\", %lu ms on air\r\n", len, msg, (unsigned long)toa_ms);
}

static void handle_key(int key) {
	switch (key) {
	case 't': transmit_test(); break;
	case 'n': Serial.printf("noise floor %.1f dBm\r\n", radio->current_rssi()); break;
	case 'a': Serial.printf("AGC reset: %d\r\n", radio->reset_agc()); break;
	case 's': report(); break;
	}
}

void setup() {
	board.begin();
	Serial.begin(115200);

	radio = radio_create(0, board.radio_spi(0));
	int16_t state = radio ? radio->begin(TEST_CONFIG) : RADIOLIB_ERR_UNSUPPORTED;
	if (state == RADIOLIB_ERR_NONE) state = radio->start_receive();
	if (state != RADIOLIB_ERR_NONE) {
		radio_error = state;
		radio = nullptr;
	}
}

// Prints the report whenever a serial monitor connects, so nothing from boot
// is missed when the monitor attaches late
static void check_monitor() {
	static bool connected = false;
	bool now = (bool)Serial;
	if (now && !connected) {
		delay(100);  // let the host finish opening the port
		if (radio_error != RADIOLIB_ERR_NONE) Serial.printf("radio0 init failed: %d\r\n", radio_error);
		report();
	}
	connected = now;
}

void loop() {
	check_monitor();
	if (!radio) { board.idle(100); return; }

	switch (radio->poll()) {
	case Radio::EVENT_RX_DONE: {
		uint8_t buf[256];
		int16_t len = radio->read_packet(buf, sizeof(buf));
		if (len >= 0) print_packet(buf, len);
		else if (len == RADIO_ERR_NO_CRC) Serial.printf("rx dropped: header has no CRC, RSSI %.1f dBm\r\n", radio->packet_rssi());
		else Serial.printf("rx error %d, RSSI %.1f dBm\r\n", len, radio->packet_rssi());
		break;
	}
	case Radio::EVENT_TX_DONE:
		board.led(false);
		Serial.print("tx done\r\n");
		break;
	default:
		break;
	}

	while (Serial.available()) handle_key(Serial.read());
	board.idle(5);
}

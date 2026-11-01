// Rrptr: Reticulum transport node for LoRa.
//
// LoRa interface stage: brings up the board and radio 0 with fixed test
// settings and runs microReticulum as a transport node on it.
// Serial keys (temporary, until the CLI): s = status, n = noise floor,
// a = AGC reset.

#include <Arduino.h>
#if defined(RRPTR_MCU_NRF52)
#include <Adafruit_TinyUSB.h>
#endif

#include <microStore/FileSystem.h>
#include <microStore/Adapters/InternalFSFileSystem.h>
#include <microReticulum.h>

#include "board/Board.h"
#include "radio/Radio.h"
#include "lora/LoRaInterface.h"

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

// Internal flash for now; the W25Q128 comes with the storage step
static microStore::FileSystem filesystem{microStore::Adapters::InternalFSFileSystem()};

static Radio* radio = nullptr;
static int16_t radio_error = RADIOLIB_ERR_NONE;
static LoRaInterface* lora = nullptr;
static RNS::Interface lora_interface(RNS::Type::NONE);
static RNS::Reticulum reticulum(RNS::Type::NONE);

static void report() {
	Serial.printf("Rrptr %s (%s, %s) on %s, reset: %s, battery %u mV, up %lu s\r\n",
		RRPTR_VERSION, RRPTR_GIT_HASH, RRPTR_BUILD_DATE, board.name(),
		board.reset_reason(), board.battery_mv(), (unsigned long)(millis() / 1000));
	if (!radio) {
		Serial.printf("radio0 init failed: %d\r\n", radio_error);
		return;
	}
	const RadioConfig& c = radio->config();
	Serial.printf("radio0 %s: %.3f MHz, %.1f kHz, SF%u, CR4/%u, %d dBm (max %d)\r\n",
		radio->chip_name(), c.freq_hz / 1e6f, c.bandwidth_hz / 1e3f, c.sf, c.cr,
		radio->applied_power_dbm(), radio->max_output_power(c.freq_hz));
	if (RNS::Transport::identity()) {
		Serial.printf("transport identity %s\r\n", RNS::Transport::identity().hash().toHex().c_str());
		Serial.printf("probe destination  %s\r\n", RNS::Transport::probe_destination().hash().toHex().c_str());
	}
	if (lora) {
		const LoRaInterface::Stats& s = lora->stats();
		Serial.printf("lora: rx %lu packets / %lu frames, errors %lu, no-crc %lu, orphans %lu; "
			"tx %lu packets, dropped %lu, airtime %lu ms, queued %lu\r\n",
			s.rx_packets, s.rx_frames, s.rx_errors, s.rx_no_crc, s.rx_orphans,
			s.tx_packets, s.tx_dropped, s.tx_airtime_ms, lora->queued());
	}
}

static void handle_key(int key) {
	switch (key) {
	case 's': report(); break;
	case 'n': if (radio) Serial.printf("noise floor %.1f dBm\r\n", radio->current_rssi()); break;
	case 'a': if (radio) Serial.printf("AGC reset: %d\r\n", radio->reset_agc()); break;
	}
}

static void reticulum_setup() {
	try {
		filesystem.init();
		RNS::Utilities::OS::register_filesystem(filesystem);

		lora = new LoRaInterface(*radio, "LoRa0");
		lora_interface = lora;
		lora_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(lora_interface);
		lora_interface.start();

		reticulum = RNS::Reticulum();
		reticulum.transport_enabled(true);
		reticulum.probe_destination_enabled(true);
		reticulum.remote_management_enabled(true);
		reticulum.start();
	}
	catch (const std::exception& e) {
		ERRORF("Reticulum setup failed: %s", e.what());
	}
}

void setup() {
	board.begin();
	Serial.begin(115200);
	RNS::loglevel(RNS::LOG_INFO);

	radio = radio_create(0, board.radio_spi(0));
	int16_t state = radio ? radio->begin(TEST_CONFIG) : RADIOLIB_ERR_UNSUPPORTED;
	if (state == RADIOLIB_ERR_NONE) state = radio->start_receive();
	if (state != RADIOLIB_ERR_NONE) {
		radio_error = state;
		radio = nullptr;
		return;
	}
	reticulum_setup();
}

// Prints the report whenever a serial monitor connects, so nothing from boot
// is missed when the monitor attaches late
static void check_monitor() {
	static bool connected = false;
	bool now = (bool)Serial;
	if (now && !connected) {
		delay(100);  // let the host finish opening the port
		report();
	}
	connected = now;
}

void loop() {
	check_monitor();
	while (Serial.available()) handle_key(Serial.read());
	if (!radio) { board.idle(100); return; }

	reticulum.loop();  // also runs the LoRa interface
	board.idle(2);
}

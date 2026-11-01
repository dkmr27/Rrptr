// Rrptr: Reticulum transport node for LoRa.
//
// Settings stage: settings live in the provisioning engine on the W25Q128;
// the radio starts from them, or stays off until a frequency is set.
// Serial keys (temporary, until the CLI): s = status, n = noise floor,
// a = AGC reset, 1 = store the test radio settings, 0 = radio off (frequency 0),
// r = reboot.

#include <Arduino.h>
#if defined(RRPTR_MCU_NRF52)
#include <Adafruit_TinyUSB.h>
#endif

#include <microReticulum.h>
#include <microReticulum/Provisioning/Provisioning.h>

#include "board/Board.h"
#include "radio/Radio.h"
#include "lora/LoRaInterface.h"
#include "storage/Storage.h"
#include "settings/Settings.h"

using RNS::Provisioning::Provisioner;
using RNS::Provisioning::Value;

static Radio* radio = nullptr;
static int16_t radio_error = RADIOLIB_ERR_NONE;
static LoRaInterface* lora = nullptr;
static RNS::Interface lora_interface(RNS::Type::NONE);
static RNS::Reticulum reticulum(RNS::Type::NONE);

static void report() {
	Serial.printf("Rrptr %s (%s, %s) on %s, reset: %s, battery %u mV, up %lu s\r\n",
		RRPTR_VERSION, RRPTR_GIT_HASH, RRPTR_BUILD_DATE, board.name(),
		board.reset_reason(), board.battery_mv(), (unsigned long)(millis() / 1000));
	Serial.printf("storage: %s\r\n", storage_description());
	if (!radio) {
		Serial.printf("radio0 init failed: %d\r\n", radio_error);
		return;
	}
	Serial.printf("node name %s\r\n", settings::node_name().c_str());
	if (settings::radio0.freq_hz == 0) {
		Serial.printf("radio0 %s: not configured, radio off (use set radio)\r\n", radio->chip_name());
	} else {
		const RadioConfig& c = radio->config();
		Serial.printf("radio0 %s: %.3f MHz, %.1f kHz, SF%u, CR4/%u, %d dBm (max %d), %s\r\n",
			radio->chip_name(), c.freq_hz / 1e6f, c.bandwidth_hz / 1e3f, c.sf, c.cr,
			radio->applied_power_dbm(), radio->max_output_power(c.freq_hz),
			lora && lora->online() ? "online" : "offline");
	}
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

// TEMPORARY until the CLI: write radio settings through the provisioning
// engine, as `set radio` will
static void store_radio(uint32_t freq_hz, uint32_t bw_hz, uint8_t sf, uint8_t cr, int8_t dbm) {
	Provisioner& p = Provisioner::instance();
	using namespace settings::radio;
	p.field(Id0, Frequency, Value((long long)freq_hz));
	p.field(Id0, Bandwidth, Value((long long)bw_hz));
	p.field(Id0, SpreadingFactor, Value((int)sf));
	p.field(Id0, CodingRate, Value((int)cr));
	p.field(Id0, TxPower, Value((int)dbm));
	Serial.printf("commit: %s\r\n", p.commit(Id0) ? "ok" : "failed");
}

static void handle_key(int key) {
	switch (key) {
	case 's': report(); break;
	case 'n': if (radio) Serial.printf("noise floor %.1f dBm\r\n", radio->current_rssi()); break;
	case 'a': if (radio) Serial.printf("AGC reset: %d\r\n", radio->reset_agc()); break;
	case '1': store_radio(925875000, 250000, 9, 5, 10); break;
	case '0': {
		Provisioner::instance().field(settings::radio::Id0, settings::radio::Frequency, Value(0));
		Serial.printf("commit: %s\r\n", Provisioner::instance().commit(settings::radio::Id0) ? "ok" : "failed");
		break;
	}
	case 'r': board.reboot(); break;
	}
}

// Brings the radio in line with the settings: off while the frequency is 0,
// otherwise configured and receiving
static void apply_radio() {
	if (settings::radio0.freq_hz == 0) {
		radio->standby();
		lora->set_online(false);
		INFO("Radio 0 not configured, radio off");
		return;
	}
	int16_t state = radio->configure(settings::radio0);
	if (state == RADIOLIB_ERR_NONE) state = radio->start_receive();
	if (state != RADIOLIB_ERR_NONE) {
		ERRORF("Radio 0 settings could not be applied (%d), radio off", state);
		radio->standby();
		lora->set_online(false);
		return;
	}
	lora->set_online(true);
	const RadioConfig& c = radio->config();
	INFOF("Radio 0 on %.3f MHz, %.1f kHz, SF%u, CR4/%u, %d dBm", c.freq_hz / 1e6f,
		c.bandwidth_hz / 1e3f, c.sf, c.cr, radio->applied_power_dbm());
}

static void reticulum_setup() {
	try {
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
		apply_radio();
	}
	catch (const std::exception& e) {
		ERRORF("Reticulum setup failed: %s", e.what());
	}
}

void setup() {
	board.begin();
	Serial.begin(115200);
	RNS::loglevel(RNS::LOG_INFO);

	// Without storage microReticulum cannot keep its identity or tables, so
	// nothing else starts; loop() keeps reporting the error
	if (!storage_begin()) return;
	RNS::Utilities::OS::register_filesystem(storage_filesystem());

	// Load the settings before anything uses them. Reticulum::start() would
	// start the provisioning engine too, but the radio needs its settings first.
	radio = radio_create(0, board.radio_spi(0));
	settings::attach_radio(radio);
	settings::register_namespaces();
	Provisioner::instance().begin();

	// Bring the chip up; a blank node uses the band's lowest frequency as a
	// placeholder and apply_radio() then leaves the radio off
	RadioConfig boot = settings::radio0;
	if (radio && boot.freq_hz == 0) {
		const RadioBandInfo* bands = nullptr;
		radio->bands(bands);
		boot.freq_hz = bands[0].freq_min_hz;
	}
	int16_t state = radio ? radio->begin(boot) : RADIOLIB_ERR_UNSUPPORTED;
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

// Storage failed at boot: repeat the error every 10 s, nothing else runs
static void storage_failed() {
	static uint32_t last = 0;
	if (last == 0 || millis() - last >= 10000) {
		last = millis();
		Serial.printf("ERROR: storage: %s. Rrptr cannot run without it.\r\n", storage_error());
	}
	board.idle(100);
}

void loop() {
	if (storage_error()) { storage_failed(); return; }
	check_monitor();
	while (Serial.available()) handle_key(Serial.read());
	if (!radio) { board.idle(100); return; }

	if (settings::take_radio_changed()) apply_radio();

	// Periodic AGC reset, between frames
	static uint32_t last_agc = millis();
	if (settings::agc_reset_minutes > 0 && millis() - last_agc >= settings::agc_reset_minutes * 60000UL
			&& !radio->transmitting() && !radio->receiving_packet()) {
		last_agc = millis();
		radio->reset_agc();
	}

	reticulum.loop();  // also runs the LoRa interface
	board.idle(2);
}

// Rrptr: Reticulum transport node for LoRa.
//
// Start-up: storage, settings, radio, then microReticulum as a transport node
// on the LoRa interface. The serial port runs the CLI (type `help`).

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
#include "cli/Cli.h"
#include "log/Log.h"
#include "console/Console.h"
#include "display/Display.h"
#include "neighbours/Neighbours.h"
#include "nomadnet/NomadNet.h"
#include "reboot/Reboot.h"

using RNS::Provisioning::Provisioner;

static Radio* radio = nullptr;
static int16_t radio_error = RADIOLIB_ERR_NONE;
static LoRaInterface* lora = nullptr;
static RNS::Interface lora_interface(RNS::Type::NONE);
static RNS::Reticulum reticulum(RNS::Type::NONE);

// Brings the radio in line with the settings: off while the frequency is 0,
// otherwise configured and receiving
static void apply_radio() {
	if (settings::radio0.freq_hz == 0) {
		radio->standby();
		lora->set_online(false);
		R_INFO("Radio 0 not configured, radio off");
		return;
	}
	int16_t state = radio->configure(settings::radio0);
	if (state == RADIOLIB_ERR_NONE) state = radio->start_receive();
	if (state != RADIOLIB_ERR_NONE) {
		R_ERROR("Radio 0 settings could not be applied (%d), radio off", state);
		radio->standby();
		lora->set_online(false);
		return;
	}
	lora->set_online(true);
	const RadioConfig& c = radio->config();
	R_INFO("Radio 0 on %.3f MHz, %.1f kHz, SF%u, CR4/%u, %d dBm", c.freq_hz / 1e6f,
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
		reticulum.start();
		apply_radio();
		neighbours::begin(lora_interface);
		nomadnet::begin(radio, lora);
	}
	catch (const std::exception& e) {
		R_ERROR("Reticulum setup failed: %s", e.what());
	}
}

void setup() {
	board.begin();
	console.begin(115200);
	rlog::begin();

	// Without storage microReticulum cannot keep its identity or tables, so
	// nothing else starts; loop() keeps reporting the error
	if (!storage_begin()) return;
	RNS::Utilities::OS::register_filesystem(storage_filesystem());

	// A transport node's defaults. These must be set before the provisioning
	// engine starts: its General Config fields take their defaults from these
	// values, and saved settings are then applied on top.
	RNS::Reticulum::transport_enabled(true);
	RNS::Reticulum::probe_destination_enabled(true);
	RNS::Reticulum::remote_management_enabled(true);

	// Load the settings before anything uses them. Reticulum::start() would
	// start the provisioning engine too, but the radio needs its settings first.
	radio = radio_create(0, board.radio_spi(0));
	settings::attach_radio(radio);
	settings::register_namespaces();
	Provisioner::instance().on_reboot([]() { reboot::request(); });
	Provisioner::instance().begin();
	settings::clamp_stored_power();

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
	} else {
		reticulum_setup();
	}
	cli::begin(radio, lora);
	display::begin(radio, lora);
}

static void greet() {
	console.printf("\r\nRrptr %s on %s, %s\r\n", RRPTR_VERSION, board.name(), settings::node_name().c_str());
	if (!radio) console.printf("radio0 init failed (%d)\r\n", radio_error);
	else if (settings::radio0.freq_hz == 0) console.print("radio not configured: set radio <MHz>,<kHz>,<SF>,<CR>[,<dBm>]\r\n");
	console.print("type help for commands\r\n> ");
}

// Greets a serial monitor when it connects, so it sees the node's state even
// when it attaches long after boot. A UART cannot tell when a terminal
// attaches, so it is greeted once at start-up.
static void check_monitor() {
	static bool connected = false;
	static bool uart_greeted = false;
	if (!uart_greeted && console.has_uart()) { uart_greeted = true; greet(); }
	bool now = console.usb_connected();
	if (now && !connected) {
		delay(100);  // let the host finish opening the port
		greet();
	}
	connected = now;
}

// Storage failed at boot: repeat the error every 10 s, nothing else runs
static void storage_failed() {
	static uint32_t last = 0;
	if (last == 0 || millis() - last >= 10000) {
		last = millis();
		console.printf("ERROR: storage: %s. Rrptr cannot run without it.\r\n", storage_error());
	}
	board.idle(100);
}

void loop() {
	if (storage_error()) { storage_failed(); return; }
	check_monitor();
	cli::poll();
	reboot::check(radio, lora);
	display::update();
	if (!radio) { board.idle(10); return; }

	if (settings::take_radio_changed()) apply_radio();

	// Periodic AGC reset, between frames
	static uint32_t last_agc = millis();
	if (settings::agc_reset_minutes > 0 && millis() - last_agc >= settings::agc_reset_minutes * 60000UL
			&& !radio->transmitting() && !radio->scanning() && !radio->receiving_packet()) {
		last_agc = millis();
		radio->reset_agc();
	}

	reticulum.loop();  // also runs the LoRa interface
	nomadnet::loop();

	// Sleep until the radio interrupts or the idle time is up. Packets waiting
	// to go out (e.g. held off by listen-before-talk) are retried every 2 ms.
	board.idle(lora && lora->queued() ? 2 : cli::loop_idle_ms);
}

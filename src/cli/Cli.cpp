#include "cli/Cli.h"

#include <Arduino.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <vector>

#include <microReticulum/Transport.h>
#include <microReticulum/Identity.h>
#include <microReticulum/Provisioning/Provisioning.h>
#include <microReticulum/Provisioning/Ids.h>

#include "board/Board.h"
#include "radio/Radio.h"
#include "lora/LoRaInterface.h"
#include "settings/Settings.h"
#include "storage/Storage.h"
#include "log/Log.h"
#include "console/Console.h"

using RNS::Provisioning::Provisioner;
using RNS::Provisioning::Value;
namespace GeneralConfig = RNS::Provisioning::Ns::GeneralConfig;

namespace cli {

static const size_t LINE_MAX = 128;
static const size_t ADMIN_MAX = 32;     // the library's limit for the allowed list

uint32_t loop_idle_ms = 50;

static Radio* radio = nullptr;
static LoRaInterface* lora = nullptr;
static char line[LINE_MAX + 1];
static size_t line_len = 0;
static bool erase_armed = false;

void begin(Radio* r, LoRaInterface* l) {
	radio = r;
	lora = l;
}

// ─── Output helpers ─────────────────────────────────────────
static void reply(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void reply(const char* fmt, ...) {
	char buf[192];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	console.print(buf);
	console.print("\r\n");
}

static void prompt() { console.print("> "); }

static bool commit(uint16_t ns) {
	if (Provisioner::instance().commit(ns)) return true;
	reply("Error: could not save the setting");
	return false;
}

static bool set_int(uint16_t ns, uint16_t field, long long value) {
	if (!Provisioner::instance().field(ns, field, Value(value))) {
		reply("Error: value out of range");
		return false;
	}
	return true;
}

static bool parse_on_off(const char* arg, bool& out) {
	if (!arg) return false;
	if (!strcmp(arg, "on"))  { out = true;  return true; }
	if (!strcmp(arg, "off")) { out = false; return true; }
	return false;
}

// 32 hex characters -> 16-byte hash
static bool parse_hash(const char* hex, RNS::Bytes& out) {
	if (!hex || strlen(hex) != 32) return false;
	uint8_t raw[16];
	for (int i = 0; i < 16; i++) {
		char byte[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
		char* end;
		raw[i] = strtoul(byte, &end, 16);
		if (*end) return false;
	}
	out = RNS::Bytes(raw, sizeof(raw));
	return true;
}

static void print_radio() {
	const RadioConfig& c = settings::radio0;
	if (c.freq_hz == 0) {
		reply("radio not configured (set radio <MHz>,<kHz>,<SF>,<CR>[,<dBm>])");
		return;
	}
	char bw[16];
	snprintf(bw, sizeof(bw), "%g", c.bandwidth_hz / 1000.0);
	reply("%.3f,%s,%u,%u,%d", c.freq_hz / 1e6, bw, c.sf, c.cr, c.power_dbm);
}

// ─── Commands ───────────────────────────────────────────────
static void cmd_ver() {
	reply("Rrptr %s (%s, built %s)", RRPTR_VERSION, RRPTR_GIT_HASH, RRPTR_BUILD_DATE);
	reply("microReticulum %s", RRPTR_URTN_REV);
}

static void cmd_hw() {
	reply("board %s", board.name());
#if defined(RRPTR_MCU_NRF52)
	reply("mcu nRF52840");
#elif defined(RRPTR_MCU_ESP32)
	reply("mcu ESP32");
#endif
	if (radio) {
		const RadioConfig& c = radio->config();
		reply("radio0 %s, max %d dBm", radio->chip_name(), radio->max_output_power(c.freq_hz));
	} else {
		reply("radio0 not available");
	}
	reply("storage %s", storage_description());
}

static void cmd_bat() {
	reply("%u mV", board.battery_mv());
}

static void cmd_stats() {
	uint32_t up = millis() / 1000;
	reply("uptime %lud %02lu:%02lu:%02lu, reset: %s", (unsigned long)(up / 86400),
		(unsigned long)(up / 3600 % 24), (unsigned long)(up / 60 % 60), (unsigned long)(up % 60),
		board.reset_reason());
	reply("battery %u mV", board.battery_mv());
	if (radio && lora) {
		const LoRaInterface::Stats& s = lora->stats();
		if (lora->online()) {
			reply("radio online, last RSSI %.1f dBm, SNR %.2f dB, noise floor %.1f dBm",
				radio->packet_rssi(), radio->packet_snr(), radio->current_rssi());
		} else {
			reply("radio off");
		}
		float airtime_pct = up ? s.tx_airtime_ms / 10.0f / up : 0;
		reply("rx %lu packets (%lu frames), errors %lu, no-crc %lu, orphans %lu",
			s.rx_packets, s.rx_frames, s.rx_errors, s.rx_no_crc, s.rx_orphans);
		reply("tx %lu packets, dropped %lu, queued %lu, airtime %lu ms (%.2f%%)",
			s.tx_packets, s.tx_dropped, lora->queued(), s.tx_airtime_ms, airtime_pct);
	}
	reply("paths %u, destinations %u, announces %u, links %u",
		(unsigned)RNS::Transport::new_path_table().size(),
		(unsigned)RNS::Identity::known_destinations().size(),
		(unsigned)RNS::Transport::announce_table().size(),
		(unsigned)RNS::Transport::active_links().size());
}

// Transport identity plus the destinations the node announces
static void cmd_get_id() {
	if (!RNS::Transport::identity()) { reply("Error: no identity yet"); return; }
	const RNS::Destination& mgmt = RNS::Transport::remote_management_destination();
	const RNS::Destination& probe = RNS::Transport::probe_destination();
	reply("transport %s", RNS::Transport::identity().hash().toHex().c_str());
	reply("mgmt      %s", mgmt ? mgmt.hash().toHex().c_str() : "off");
	reply("probe     %s", probe ? probe.hash().toHex().c_str() : "off");
}

// announce [mgmt|probe]: announce now instead of waiting for microReticulum's
// own management announce (every 2 hours, the first 2 hours after boot)
static void cmd_announce(char* arg) {
	bool mgmt = !arg || !strcmp(arg, "mgmt");
	bool probe = !arg || !strcmp(arg, "probe");
	if (!mgmt && !probe) { reply("Error: announce [mgmt|probe]"); return; }
	if (!lora || !lora->online()) { reply("Error: radio off"); return; }

	int sent = 0;
	try {
		if (mgmt && RNS::Transport::remote_management_destination()) {
			RNS::Destination d = RNS::Transport::remote_management_destination();
			d.announce();
			sent++;
		}
		if (probe && RNS::Transport::probe_destination()) {
			RNS::Destination d = RNS::Transport::probe_destination();
			d.announce();
			sent++;
		}
	}
	catch (const std::exception& e) {
		reply("Error: %s", e.what());
		return;
	}
	if (sent) reply("OK, %d announce%s queued", sent, sent > 1 ? "s" : "");
	else reply("Error: nothing to announce");
}

// prov | prov <hash> | prov del <hash>
static void cmd_prov(char* arg1, char* arg2) {
	Provisioner& p = Provisioner::instance();
	std::vector<RNS::Bytes> list =
		p.field(GeneralConfig::Id, GeneralConfig::Field::RemoteManagementAllowed).as_bytes_list();

	if (!arg1) {
		if (list.empty()) reply("no remote admin identities");
		for (const RNS::Bytes& h : list) reply("%s", h.toHex().c_str());
		return;
	}

	bool remove = !strcmp(arg1, "del");
	RNS::Bytes hash;
	if (!parse_hash(remove ? arg2 : arg1, hash)) {
		reply("Error: expected a 32 character identity hash");
		return;
	}
	auto found = std::find(list.begin(), list.end(), hash);
	if (remove) {
		if (found == list.end()) { reply("Error: not in the list"); return; }
		list.erase(found);
	} else {
		if (found != list.end()) { reply("already in the list"); return; }
		if (list.size() >= ADMIN_MAX) { reply("Error: list is full (%u)", (unsigned)ADMIN_MAX); return; }
		list.push_back(hash);
	}
	if (!p.field(GeneralConfig::Id, GeneralConfig::Field::RemoteManagementAllowed, Value(list))) {
		reply("Error: could not update the list");
		return;
	}
	if (commit(GeneralConfig::Id)) reply("OK, reboot to apply");
}

// set radio <MHz>,<kHz>,<SF>,<CR>[,<dBm>]
static void cmd_set_radio(char* arg) {
	if (!arg) { reply("Error: set radio <MHz>,<kHz>,<SF>,<CR>[,<dBm>]"); return; }
	double values[5];
	int count = 0;
	for (char* tok = strtok(arg, ","); tok && count < 5; tok = strtok(nullptr, ",")) {
		char* end;
		values[count] = strtod(tok, &end);
		if (end == tok || *end) { reply("Error: '%s' is not a number", tok); return; }
		count++;
	}
	if (count < 4) { reply("Error: set radio <MHz>,<kHz>,<SF>,<CR>[,<dBm>]"); return; }

	RadioConfig c = settings::radio0;
	c.freq_hz = (uint32_t)(values[0] * 1e6 + 0.5);
	c.bandwidth_hz = (uint32_t)(values[1] * 1e3 + 0.5);
	c.sf = (uint8_t)values[2];
	c.cr = (uint8_t)values[3];
	if (count == 5) c.power_dbm = (int8_t)values[4];
	// Check here for a clear message; the settings commit hook checks again
	if (radio && !radio->config_supported(c)) {
		reply("Error: not supported by this radio, nothing changed");
		return;
	}

	using namespace settings::radio;
	if (!set_int(Id0, Frequency, c.freq_hz) || !set_int(Id0, Bandwidth, c.bandwidth_hz)
			|| !set_int(Id0, SpreadingFactor, c.sf) || !set_int(Id0, CodingRate, c.cr)
			|| !set_int(Id0, TxPower, c.power_dbm)) {
		Provisioner::instance().discard(Id0);
		return;
	}
	if (!commit(Id0)) return;
	// The commit stores power above the board's ceiling as the ceiling
	if (settings::radio0.power_dbm < c.power_dbm) {
		reply("OK, TX power limited to %d dBm on this board", settings::radio0.power_dbm);
	} else {
		reply("OK");
	}
}

static void cmd_set(char* what, char* arg) {
	using namespace settings;
	if (!what) { reply("Error: set what? (type help)"); return; }

	if (!strcmp(what, "radio")) { cmd_set_radio(arg); return; }

	if (!strcmp(what, "tx")) {
		if (!arg) { reply("Error: set tx <dBm>"); return; }
		int dbm = atoi(arg);
		if (!set_int(radio::Id0, radio::TxPower, dbm) || !commit(radio::Id0)) return;
		// The commit stores power above the board's ceiling as the ceiling
		if (radio0.power_dbm < dbm) reply("OK, limited to %d dBm on this board", radio0.power_dbm);
		else reply("OK");
		return;
	}

	if (!strcmp(what, "name")) {
		if (!arg) { reply("Error: set name <name>"); return; }
		if (!Provisioner::instance().field(node::Id, node::Name, Value(arg))) { reply("Error: name too long (max 32)"); return; }
		if (commit(node::Id)) reply("OK");
		return;
	}

	if (!strcmp(what, "agcreset")) {
		long minutes = (arg && !strcmp(arg, "off")) ? 0 : (arg ? atol(arg) : -1);
		if (minutes < 0) { reply("Error: set agcreset <minutes>|0|off"); return; }
		if (set_int(radio::Id0, radio::AgcResetMinutes, minutes) && commit(radio::Id0)) reply("OK");
		return;
	}

	if (!strcmp(what, "rxgain")) {
		bool on;
		if (!parse_on_off(arg, on)) { reply("Error: set rxgain on|off"); return; }
		if (Provisioner::instance().field(radio::Id0, radio::RxBoostedGain, Value(on)) && commit(radio::Id0)) reply("OK");
		return;
	}

	if (!strcmp(what, "led")) {
		int mode = (arg && !strcmp(arg, "tx")) ? LED_TX : (arg && !strcmp(arg, "off")) ? LED_OFF : -1;
		if (mode < 0) { reply("Error: set led off|tx"); return; }
		if (Provisioner::instance().field(node::Id, node::LedMode, Value::make_enum(mode)) && commit(node::Id)) reply("OK");
		return;
	}

	reply("Error: unknown setting '%s' (type help)", what);
}

static void cmd_get(char* what) {
	using namespace settings;
	if (!what) { reply("Error: get what? (type help)"); return; }
	if (!strcmp(what, "radio"))    { print_radio(); return; }
	if (!strcmp(what, "tx"))       { reply("%d dBm", radio0.power_dbm); return; }
	if (!strcmp(what, "id"))       { cmd_get_id(); return; }
	if (!strcmp(what, "name"))     { reply("%s", node_name().c_str()); return; }
	if (!strcmp(what, "agcreset")) { if (agc_reset_minutes) reply("%u min", agc_reset_minutes); else reply("off"); return; }
	if (!strcmp(what, "led"))      { reply("%s", led_mode == LED_TX ? "tx" : "off"); return; }
	if (!strcmp(what, "rxgain"))   { reply("%s", radio0.rx_boosted_gain ? "on" : "off"); return; }
	reply("Error: unknown setting '%s' (type help)", what);
}

static void cmd_log(char* arg) {
	if (!arg) { reply("%s", rlog::mode_name(rlog::mode())); return; }
	if (!strcmp(arg, "off"))       rlog::set_mode(rlog::OFF);
	else if (!strcmp(arg, "on"))   rlog::set_mode(rlog::ON);
	else if (!strcmp(arg, "both")) rlog::set_mode(rlog::BOTH);
	else { reply("Error: log on|both|off"); return; }
	reply("OK");
}

static void cmd_erase(char* arg) {
	if (!arg || strcmp(arg, "yes")) {
		reply("This wipes all settings, the transport identity and all tables.");
		reply("Type 'erase yes' to confirm.");
		erase_armed = true;
		return;
	}
	if (!erase_armed) { reply("Type 'erase' first"); return; }
	reply("Erasing storage and rebooting...");
	console.flush();
	storage_filesystem().format();
	delay(100);
	board.reboot();
}

#if defined(RRPTR_DEBUG_LOG)
// Test commands for power measurements; not saved, a reboot undoes them.
//   test radio rx|standby|sleep   put the radio in that state (LoRa offline
//                                 unless rx)
//   test idle <ms>                longest main loop idle per pass (default 50)
//   test uart off                 switch the console UART off (USB stays)
static void cmd_test(char* what, char* arg) {
	if (what && !strcmp(what, "radio") && arg) {
		if (!radio || !lora) { reply("Error: no radio"); return; }
		int16_t state;
		if (!strcmp(arg, "rx")) {
			state = radio->standby();
			if (state == RADIOLIB_ERR_NONE) state = radio->start_receive();
			if (state == RADIOLIB_ERR_NONE) lora->set_online(settings::radio0.freq_hz != 0);
		} else if (!strcmp(arg, "standby") || !strcmp(arg, "sleep")) {
			lora->set_online(false);
			state = !strcmp(arg, "sleep") ? radio->sleep() : radio->standby();
		} else {
			reply("Error: test radio rx|standby|sleep");
			return;
		}
		if (state == RADIOLIB_ERR_NONE) reply("OK");
		else reply("Error: radio %d", state);
		return;
	}
	if (what && !strcmp(what, "idle") && arg) {
		long ms = atol(arg);
		if (ms < 0 || ms > 1000) { reply("Error: test idle 0..1000"); return; }
		loop_idle_ms = ms;
		reply("OK");
		return;
	}
	if (what && !strcmp(what, "uart") && arg && !strcmp(arg, "off")) {
		if (!console.has_uart()) { reply("UART already off"); return; }
		reply("UART off until reboot");
		console.end_uart();
		return;
	}
	reply("Error: test radio rx|standby|sleep | idle <ms> | uart off");
}
#endif

static void cmd_help() {
	reply("ver                         firmware and microReticulum version");
	reply("hw                          board, radio and storage");
	reply("stats                       uptime, battery, radio and traffic counters");
	reply("bat                         battery voltage");
	reply("get id                      transport identity and destination hashes");
	reply("an|announce [mgmt|probe]    announce now (default both)");
	reply("prov [<hash> | del <hash>]  list, add or remove remote admin identities");
	reply("set radio <MHz>,<kHz>,<SF>,<CR>[,<dBm>]");
	reply("set|get tx <dBm>            chip output power");
	reply("set|get name <name>         node name");
	reply("set|get agcreset <min>|off  AGC reset interval");
	reply("set|get led off|tx          LED on transmit");
	reply("set|get rxgain on|off       RX boosted gain (better sensitivity, more current)");
	reply("get radio                   radio settings");
	if (rlog::debug_build()) reply("log [on|both|off]           log output (debug build: both at boot)");
	else reply("log [on|both|off]           log output (off after every boot)");
	reply("reboot                      restart");
	reply("erase                       factory reset (asks to confirm)");
	reply("ota                         firmware update (not implemented yet)");
#if defined(RRPTR_DEBUG_LOG)
	reply("test radio rx|standby|sleep power test: radio state (until reboot)");
	reply("test idle <ms>              power test: longest main loop idle (default 50)");
	reply("test uart off               power test: console UART off (until reboot)");
#endif
}

// ─── Dispatch ───────────────────────────────────────────────
static void run(char* text) {
	char* cmd = strtok(text, " ");
	if (!cmd) return;
	char* arg1 = strtok(nullptr, " ");
	char* arg2 = strtok(nullptr, "");   // the rest of the line (names may contain spaces)
	if (strcmp(cmd, "erase")) erase_armed = false;

	if (!strcmp(cmd, "ver") || !strcmp(cmd, "version")) cmd_ver();
	else if (!strcmp(cmd, "hw") || !strcmp(cmd, "board")) cmd_hw();
	else if (!strcmp(cmd, "stats")) cmd_stats();
	else if (!strcmp(cmd, "bat")) cmd_bat();
	else if (!strcmp(cmd, "prov")) cmd_prov(arg1, arg2);
	else if (!strcmp(cmd, "an") || !strcmp(cmd, "announce")) cmd_announce(arg1);
	else if (!strcmp(cmd, "set")) cmd_set(arg1, arg2);
	else if (!strcmp(cmd, "get")) cmd_get(arg1);
	else if (!strcmp(cmd, "log")) cmd_log(arg1);
	else if (!strcmp(cmd, "reboot")) { reply("Rebooting..."); console.flush(); delay(100); board.reboot(); }
	else if (!strcmp(cmd, "erase")) cmd_erase(arg1);
	else if (!strcmp(cmd, "ota")) reply("Not implemented yet");
#if defined(RRPTR_DEBUG_LOG)
	else if (!strcmp(cmd, "test")) cmd_test(arg1, arg2);
#endif
	else if (!strcmp(cmd, "help") || !strcmp(cmd, "?")) cmd_help();
	else reply("Error: unknown command '%s' (type help)", cmd);
}

void poll() {
	while (console.available()) {
		int c = console.read();
		if (c == '\r' || c == '\n') {
			if (line_len == 0) { if (c == '\r') { console.print("\r\n"); prompt(); } continue; }
			console.print("\r\n");
			line[line_len] = 0;
			run(line);
			line_len = 0;
			prompt();
		} else if (c == 8 || c == 127) {
			if (line_len > 0) { line_len--; console.print("\b \b"); }
		} else if (c >= 32 && c < 127 && line_len < LINE_MAX) {
			line[line_len++] = (char)c;
			console.write((char)c);
		}
	}
}

}

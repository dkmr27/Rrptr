#include "nomadnet/NomadNet.h"

#if RRPTR_NOMADNET

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>
#include <vector>

#include <microReticulum/Identity.h>
#include <microReticulum/Transport.h>
#include <microReticulum/Provisioning/Provisioning.h>
#include <microReticulum/Provisioning/Ids.h>

#include "board/Board.h"
#include "log/Log.h"
#include "lora/LoRaInterface.h"
#include "neighbours/Neighbours.h"
#include "radio/Radio.h"
#include "settings/Settings.h"

using RNS::Bytes;
using RNS::Identity;

namespace nomadnet {

// As microReticulum's management announce
static const uint32_t ANNOUNCE_INTERVAL_MS = 2UL * 60 * 60 * 1000;
static const size_t HEADER_LEN = 3;     // msgpack bin16 header

static RNS::Destination dest(RNS::Type::NONE);
static Radio* radio = nullptr;
static LoRaInterface* lora = nullptr;
static uint32_t last_announce = 0;

// One page is built at a time, as requests are handled in the main loop
static char page[1400];
static size_t page_len = 0;

static void add(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void add(const char* fmt, ...) {
	size_t room = sizeof(page) - HEADER_LEN - page_len;
	va_list args;
	va_start(args, fmt);
	int n = vsnprintf(page + HEADER_LEN + page_len, room, fmt, args);
	va_end(args);
	if (n > 0) page_len += std::min((size_t)n, room - 1);
}

// #!c=0 stops the client caching the page, as the numbers change. Micron is
// written with no blank line after the heading, so it stays compact.
static void start_page(const char* title) {
	page_len = 0;
	add("#!c=0\n>%s\n", title);
}

// The library expects a response already in msgpack; clients expect bytes
static Bytes finish_page() {
	page[0] = 0xc5;
	page[1] = page_len >> 8;
	page[2] = page_len & 0xff;
	return Bytes((const uint8_t*)page, HEADER_LEN + page_len);
}

static void add_back_link() { add("\n`[Back`:/page/index.mu]\n"); }

static void add_uptime() {
	uint32_t up = millis() / 1000;
	add("Uptime: %lud %02lu:%02lu:%02lu\n", (unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
		(unsigned long)(up / 60 % 60), (unsigned long)(up % 60));
}

static bool is_admin(const Identity& identity) {
	if (!identity) return false;
	std::vector<Bytes> list = RNS::Provisioning::Provisioner::instance()
		.field(RNS::Provisioning::Ns::GeneralConfig::Id,
			RNS::Provisioning::Ns::GeneralConfig::Field::RemoteManagementAllowed).as_bytes_list();
	return std::find(list.begin(), list.end(), identity.hash()) != list.end();
}

// The answer to a request that failed the admin check, instead of no response,
// which a client can only show as a timeout
static Bytes denied_page(const Identity& identity) {
	start_page("Admin only");
	if (identity) {
		add("Your identity %s is not on this node's remote management list.\n", identity.hash().toHex().c_str());
	} else {
		add("This page needs an identified link. Identify to this node in your client, then reload.\n");
	}
	add_back_link();
	return finish_page();
}

// ─── Pages ──────────────────────────────────────────────────
static Bytes index_page(const Bytes&, const Bytes&, const Bytes&, const Bytes&, const Identity&, double) {
	char name[neighbours::NAME_LEN + 1];
	std::string node = settings::node_name();
	neighbours::clean_text(node.data(), node.size(), name, sizeof(name));
	start_page("Rrptr");
	add("%s\n\n", name);
	add("`[General statistics`:/page/stats.mu]\n");
	add("`[Admin statistics`:/page/admin.mu]\n");
	return finish_page();
}

static Bytes stats_page(const Bytes&, const Bytes&, const Bytes&, const Bytes&, const Identity&, double) {
	start_page("General statistics");
	add("Firmware: Rrptr %s\n", RRPTR_VERSION);
	add_uptime();
	const RadioConfig& c = settings::radio0;
	if (radio && lora && lora->online()) {
		add("Radio: %.3f MHz, %g kHz, SF%u, CR4/%u, %d dBm\n", c.freq_hz / 1e6, c.bandwidth_hz / 1000.0, c.sf, c.cr,
			radio->applied_power_dbm());
	} else {
		add("Radio: off\n");
	}
	if (lora) {
		const LoRaInterface::Stats& s = lora->stats();
		uint32_t up = millis() / 1000;
		add("Packets: %lu received, %lu sent\n", s.rx_packets, s.tx_packets);
		add("Airtime: %.2f%%\n", up ? s.tx_airtime_ms / 10.0f / up : 0.0f);
	}
	add("Paths: %u\n", (unsigned)RNS::Transport::new_path_table().size());
	add_back_link();
	return finish_page();
}

static Bytes admin_page(const Bytes&, const Bytes&, const Bytes&, const Bytes&, const Identity& identity, double) {
	if (!is_admin(identity)) return denied_page(identity);
	start_page("Admin statistics");
	add_uptime();
	add("Reset: %s\n", board.reset_reason());
	add("Battery: %u mV\n", board.battery_mv());
	if (radio && lora && lora->online()) {
		add("Last RSSI %.1f dBm, SNR %.2f dB\n", radio->packet_rssi(), radio->packet_snr());
		add("Noise floor %.1f dBm\n", radio->current_rssi());
	}
	if (lora) {
		const LoRaInterface::Stats& s = lora->stats();
		add("RX: %lu packets, %lu frames, %lu errors, %lu no CRC, %lu orphans\n", s.rx_packets, s.rx_frames, s.rx_errors,
			s.rx_no_crc, s.rx_orphans);
		add("TX: %lu packets, %lu dropped, %lu queued, channel busy %lu\n", s.tx_packets, s.tx_dropped, lora->queued(),
			s.tx_busy);
	}
	add("Paths %u, destinations %u, announces %u, links %u\n", (unsigned)RNS::Transport::new_path_table().size(),
		(unsigned)Identity::known_destinations().size(), (unsigned)RNS::Transport::announce_table().size(),
		(unsigned)RNS::Transport::active_links().size());
	add("\n`[Heard neighbours`:/page/neighbours.mu]\n");
	add_back_link();
	return finish_page();
}

static Bytes neighbours_page(const Bytes&, const Bytes&, const Bytes&, const Bytes&, const Identity& identity, double) {
	if (!is_admin(identity)) return denied_page(identity);
	start_page("Heard neighbours");
	if (neighbours::count() == 0) add("None heard since boot.\n");
	for (size_t i = 0; i < neighbours::count(); i++) {
		const neighbours::Entry& e = neighbours::get(i);
		char age[8];
		neighbours::format_age(e.heard_ms, age, sizeof(age));
		add("%s", Bytes(e.id, neighbours::ID_LEN).toHex().c_str());
		add(" %.1f dBm %.1f dB %s ago %s\n", e.rssi, e.snr, age, e.name);
	}
	add("\n`[Admin statistics`:/page/admin.mu]\n");
	return finish_page();
}

// ─── Set-up and announcing ──────────────────────────────────
void begin(Radio* r, LoRaInterface* l) {
	if (!settings::nomadnet_enabled) return;
	radio = r;
	lora = l;
	dest = RNS::Destination(RNS::Transport::identity(), RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE,
		"nomadnetwork", "node");
	const auto allow = RNS::Type::Destination::ALLOW_ALL;   // the admin pages check for themselves
	dest.register_request_handler("/page/index.mu", index_page, allow);
	dest.register_request_handler("/page/stats.mu", stats_page, allow);
	dest.register_request_handler("/page/admin.mu", admin_page, allow);
	dest.register_request_handler("/page/neighbours.mu", neighbours_page, allow);
	last_announce = millis();
	R_INFO("NomadNet page on %s", dest.hash().toHex().c_str());
}

bool announce() {
	if (!dest) return false;
	last_announce = millis();
	dest.announce(Bytes(settings::node_name()));
	return true;
}

void loop() {
	if (dest && lora && lora->online() && millis() - last_announce >= ANNOUNCE_INTERVAL_MS) announce();
}

const RNS::Destination& destination() { return dest; }

}

#endif

#include "neighbours/Neighbours.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <memory>

#include <microReticulum/Identity.h>
#include <microReticulum/Transport.h>

namespace neighbours {

static Entry entries[MAX];
static size_t used = 0;
static RNS::Interface lora_interface(RNS::Type::NONE);

size_t count() { return used; }
const Entry& get(size_t index) { return entries[index]; }

void clean_text(const char* in, size_t in_len, char* out, size_t out_size) {
	size_t n = 0;
	for (size_t i = 0; i < in_len && n + 1 < out_size; i++) {
		char c = in[i];
		if (c == '`' || c == '\\' || c < 0x20 || c > 0x7e) c = '?';
		out[n++] = c;
	}
	out[n] = 0;
}

void format_age(uint32_t heard_ms, char* out, size_t out_size) {
	uint32_t s = (millis() - heard_ms) / 1000;
	if (s < 60) snprintf(out, out_size, "%lus", (unsigned long)s);
	else if (s < 3600) snprintf(out, out_size, "%lum", (unsigned long)(s / 60));
	else if (s < 86400) snprintf(out, out_size, "%luh", (unsigned long)(s / 3600));
	else snprintf(out, out_size, "%lud", (unsigned long)(s / 86400));
}

// The name in an announce's app data, if it has one: NomadNet nodes send the
// text itself, LXMF peers a msgpack array with the name first
static void read_name(const RNS::Bytes& app_data, char* out, size_t out_size) {
	out[0] = 0;
	const uint8_t* d = app_data.data();
	size_t len = app_data.size();
	if (len == 0) return;

	if ((d[0] & 0xf0) == 0x90) {
		if (len < 2) return;
		const uint8_t* value;
		size_t value_len;
		if ((d[1] & 0xe0) == 0xa0) {                            // fixstr
			value = d + 2;
			value_len = d[1] & 0x1f;
		} else if ((d[1] == 0xc4 || d[1] == 0xd9) && len >= 3) { // bin8, str8
			value = d + 3;
			value_len = d[2];
		} else {
			return;
		}
		if (value + value_len > d + len) return;
		clean_text((const char*)value, value_len, out, out_size);
		return;
	}
	clean_text((const char*)d, len, out, out_size);
}

class Listener : public RNS::AnnounceHandler {
public:
	void received_announce(const RNS::Bytes& destination_hash, const RNS::Identity& announced_identity,
			const RNS::Bytes& app_data) override {
		if (RNS::Transport::hops_to(destination_hash) != 1 || !announced_identity) return;
		RNS::Bytes id = announced_identity.hash();
		if (id.size() != ID_LEN) return;

		// Take out the existing entry, or the oldest if there is no room, and
		// put the updated one at the front
		size_t slot = 0;
		while (slot < used && memcmp(entries[slot].id, id.data(), ID_LEN)) slot++;
		Entry e;
		if (slot < used) {
			e = entries[slot];
		} else {
			if (used < MAX) used++;
			slot = used - 1;
			memcpy(e.id, id.data(), ID_LEN);
			e.name[0] = 0;
		}
		memmove(&entries[1], &entries[0], slot * sizeof(Entry));

		char name[NAME_LEN + 1];
		read_name(app_data, name, sizeof(name));
		if (name[0]) strcpy(e.name, name);
		e.rssi = lora_interface.r_stat_rssi();
		e.snr = lora_interface.r_stat_snr();
		e.heard_ms = millis();
		entries[0] = e;
	}
};

void begin(const RNS::Interface& interface) {
	lora_interface = interface;
	RNS::Transport::register_announce_handler(std::make_shared<Listener>());
}

}

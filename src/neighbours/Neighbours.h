// Neighbours: the nodes this one hears directly. An announce that arrives at
// 1 hop came straight from the node that made it, so each entry is an identity
// with the signal of its latest announce. One node can announce several
// destinations; they share an identity and so one entry.
//
// Kept in RAM only, newest first; when full, the oldest entry is dropped.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include <microReticulum/Interface.h>

namespace neighbours {
	static const size_t MAX = 16;
	static const size_t NAME_LEN = 24;
	static const size_t ID_LEN = 16;

	struct Entry {
		uint8_t  id[ID_LEN];
		char     name[NAME_LEN + 1];     // empty until an announce carries one
		float    rssi, snr;
		uint32_t heard_ms;               // millis
	};

	// Starts listening to announces on the interface (for its signal readings)
	void begin(const RNS::Interface& interface);

	size_t count();
	const Entry& get(size_t index);     // 0 = heard most recently

	// Copies text so it is safe to show on a page: printable ASCII only, and no
	// Micron formatting characters. Names come from anyone on the air.
	void clean_text(const char* in, size_t in_len, char* out, size_t out_size);

	// Time since heard_ms, like "45s", "12m", "3h" or "2d"
	void format_age(uint32_t heard_ms, char* out, size_t out_size);
}

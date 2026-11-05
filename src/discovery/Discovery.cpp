#include "discovery/Discovery.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include <SHA256.h>

#include <microReticulum/Cryptography/HKDF.h>
#include <microReticulum/Cryptography/Random.h>
#include <microReticulum/Identity.h>
#include <microReticulum/Reticulum.h>
#include <microReticulum/Utilities/OS.h>

#include "log/Log.h"
#include "lora/LoRaInterface.h"
#include "neighbours/Neighbours.h"
#include "settings/Settings.h"

using RNS::Bytes;

namespace discovery {

// Field keys of the interface info, as in Reticulum's interface discovery
static const uint8_t KEY_NAME            = 0xFF;
static const uint8_t KEY_TRANSPORT_ID    = 0xFE;
static const uint8_t KEY_TRANSPORT_IMPL  = 0xFD;
static const uint8_t KEY_TRANSPORT_VERS  = 0xFC;
static const uint8_t KEY_INTERFACE_TYPE  = 0x00;
static const uint8_t KEY_TRANSPORT       = 0x01;
static const uint8_t KEY_LATITUDE        = 0x03;
static const uint8_t KEY_LONGITUDE       = 0x04;
static const uint8_t KEY_HEIGHT          = 0x05;
static const uint8_t KEY_FREQUENCY       = 0x09;
static const uint8_t KEY_BANDWIDTH       = 0x0A;
static const uint8_t KEY_SPREADINGFACTOR = 0x0B;
static const uint8_t KEY_CODINGRATE      = 0x0C;
static const uint8_t INFO_FIELDS         = 13;

// The type must be one receivers accept, and the on-air format is the RNode's
static const char* const INTERFACE_TYPE = "RNodeInterface";
static const char* const IMPLEMENTATION = "Rrptr";

// A stamp's work: hash a block of WORKBLOCK_ROUNDS x 256 bytes derived from the
// info, plus the stamp, for a result with STAMP_VALUE leading zero bits
static const size_t  STAMP_LEN = 32;
static const size_t  HASH_LEN = 32;
static const uint8_t WORKBLOCK_ROUNDS = 20;
static const size_t  WORKBLOCK_ROUND_LEN = 256;

static const uint32_t ANNOUNCE_INTERVAL_MS = 6UL * 60 * 60 * 1000;
static const uint32_t BOOT_ANNOUNCE_DELAY_MS = 30000;
static const uint32_t BOOT_ANNOUNCE_JITTER_STEP_MS = 60;   // times a random byte
static const uint32_t INFO_CHECK_INTERVAL_MS = 30000;
static const uint32_t WORK_SLICE_MS = 10;

static LoRaInterface* lora = nullptr;
static RNS::Destination dest(RNS::Type::NONE);
static uint32_t next_announce = 0;      // millis
static uint32_t last_info_check = 0;    // millis
static bool started = false;

static uint8_t info[200];
static size_t info_len = 0;
static uint8_t info_hash[HASH_LEN];
static uint8_t stamp[STAMP_LEN];
static bool have_info = false;
static bool stamp_ready = false;

enum Work { WORK_NONE, WORK_WORKBLOCK, WORK_SEARCH };
static Work work = WORK_NONE;
static uint8_t work_round = 0;
static uint32_t work_started = 0;
static uint32_t work_attempts = 0;
// Hash state after the workblock. The block is a whole number of SHA-256 chunks,
// so a copy of this makes each attempt one chunk of work instead of 81.
static SHA256 workblock_digest;

// ─── Interface info in msgpack ──────────────────────────────
class Writer {
public:
	Writer(uint8_t* out, size_t size) : _out(out), _size(size) {}
	size_t length() const { return _len; }
	bool ok() const { return _ok; }

	void byte(uint8_t b) {
		if (_len < _size) _out[_len++] = b;
		else _ok = false;
	}
	void key(uint8_t k) {
		if (k >= 0x80) byte(0xcc);
		byte(k);
	}
	void boolean(bool v) { byte(v ? 0xc3 : 0xc2); }
	void nil() { byte(0xc0); }
	void uint(uint32_t v) {
		if (v < 0x80) {
			byte(v);
		} else if (v <= 0xff) {
			byte(0xcc);
			byte(v);
		} else if (v <= 0xffff) {
			byte(0xcd);
			byte(v >> 8);
			byte(v);
		} else {
			byte(0xce);
			for (int shift = 24; shift >= 0; shift -= 8) byte(v >> shift);
		}
	}
	void str(const char* s) {
		size_t n = strlen(s);
		if (n < 32) {
			byte(0xa0 | n);
		} else {
			byte(0xd9);
			byte(n);
		}
		for (size_t i = 0; i < n; i++) byte(s[i]);
	}
	void bin(const uint8_t* data, size_t n) {
		byte(0xc4);
		byte(n);
		for (size_t i = 0; i < n; i++) byte(data[i]);
	}
	void float64(double v) {
		uint64_t bits;
		memcpy(&bits, &v, sizeof(bits));
		byte(0xcb);
		for (int shift = 56; shift >= 0; shift -= 8) byte(bits >> shift);
	}

private:
	uint8_t* _out;
	size_t _size;
	size_t _len = 0;
	bool _ok = true;
};

// Builds the info into `out`; returns its length, 0 if it did not fit
static size_t build_info(uint8_t* out, size_t size) {
	char name[33];
	std::string node = settings::node_name();
	neighbours::clean_text(node.data(), node.size(), name, sizeof(name));
	Bytes id = RNS::Transport::identity().hash();
	const RadioConfig& c = settings::radio0;

	Writer w(out, size);
	w.byte(0x80 | INFO_FIELDS);
	w.key(KEY_INTERFACE_TYPE);  w.str(INTERFACE_TYPE);
	w.key(KEY_TRANSPORT);       w.boolean(RNS::Reticulum::transport_enabled());
	w.key(KEY_TRANSPORT_ID);    w.bin(id.data(), id.size());
	w.key(KEY_TRANSPORT_IMPL);  w.str(IMPLEMENTATION);
	w.key(KEY_TRANSPORT_VERS);  w.str(RRPTR_VERSION);
	w.key(KEY_NAME);            w.str(name);
	w.key(KEY_LATITUDE);
	if (settings::location_enabled) w.float64(settings::latitude); else w.nil();
	w.key(KEY_LONGITUDE);
	if (settings::location_enabled) w.float64(settings::longitude); else w.nil();
	w.key(KEY_HEIGHT);
	if (settings::location_enabled) w.float64(settings::height); else w.nil();
	w.key(KEY_FREQUENCY);       w.uint(c.freq_hz);
	w.key(KEY_BANDWIDTH);       w.uint(c.bandwidth_hz);
	w.key(KEY_SPREADINGFACTOR); w.uint(c.sf);
	w.key(KEY_CODINGRATE);      w.uint(c.cr);
	return w.ok() ? w.length() : 0;
}

// ─── Stamp ──────────────────────────────────────────────────
static void stamp_path(char* out, size_t size) {
	snprintf(out, size, "%s/discovery_stamp", RNS::Reticulum::storagepath());
}

// The saved stamp, if it was found for the current info
static bool load_stamp() {
	char path[64];
	stamp_path(path, sizeof(path));
	if (!RNS::Utilities::OS::file_exists(path)) return false;
	Bytes saved;
	if (RNS::Utilities::OS::read_file(path, saved) != HASH_LEN + STAMP_LEN) return false;
	if (memcmp(saved.data(), info_hash, HASH_LEN)) return false;
	memcpy(stamp, saved.data() + HASH_LEN, STAMP_LEN);
	return true;
}

static void save_stamp() {
	char path[64];
	stamp_path(path, sizeof(path));
	Bytes saved(info_hash, HASH_LEN);
	saved << Bytes(stamp, STAMP_LEN);
	if (RNS::Utilities::OS::write_file(path, saved) != saved.size()) {
		R_WARNING("Discovery: could not save the stamp");
	}
}

static void start_work() {
	work = WORK_WORKBLOCK;
	work_round = 0;
	work_attempts = 0;
	work_started = millis();
	workblock_digest.reset();
	R_INFO("Discovery: finding a stamp");
}

// One round per call: each takes several milliseconds of HMACs
static void workblock_step() {
	Bytes material(info_hash, HASH_LEN);
	Bytes salt_material(material);
	salt_material << Bytes(&work_round, 1);     // msgpack of a small integer is itself
	Bytes salt = RNS::Identity::full_hash(salt_material);
	Bytes block = RNS::Cryptography::hkdf(WORKBLOCK_ROUND_LEN, material, salt);
	workblock_digest.update(block.data(), block.size());
	if (++work_round == WORKBLOCK_ROUNDS) {
		memcpy(stamp, RNS::Cryptography::random(STAMP_LEN).data(), STAMP_LEN);
		work = WORK_SEARCH;
	}
}

static void search_step() {
	uint32_t start = millis();
	uint8_t result[HASH_LEN];
	do {
		for (int i = 0; i < 16; i++) {
			SHA256 attempt(workblock_digest);
			attempt.update(stamp, STAMP_LEN);
			attempt.finalize(result, sizeof(result));
			work_attempts++;
			// A value of 16: the first 16 bits are zero
			if (result[0] == 0 && result[1] == 0) {
				stamp_ready = true;
				work = WORK_NONE;
				save_stamp();
				R_INFO("Discovery: stamp found in %lu ms, %lu attempts", (unsigned long)(millis() - work_started),
					(unsigned long)work_attempts);
				return;
			}
			uint32_t counter;
			memcpy(&counter, stamp, sizeof(counter));
			counter++;
			memcpy(stamp, &counter, sizeof(counter));
		}
	} while (millis() - start < WORK_SLICE_MS);
}

// Brings the stamp in line with the current info
static void check_info() {
	last_info_check = millis();
	uint8_t built[sizeof(info)];
	size_t len = build_info(built, sizeof(built));
	if (len == 0) {
		R_WARNING("Discovery: interface info too long");
		return;
	}
	Bytes hash = RNS::Identity::full_hash(Bytes(built, len));
	if (have_info && !memcmp(hash.data(), info_hash, HASH_LEN)) return;

	memcpy(info, built, len);
	info_len = len;
	memcpy(info_hash, hash.data(), HASH_LEN);
	have_info = true;
	stamp_ready = load_stamp();
	if (stamp_ready) work = WORK_NONE;
	else start_work();
}

// ─── Announcing ─────────────────────────────────────────────
static void send_announce() {
	next_announce = millis() + ANNOUNCE_INTERVAL_MS;
	Bytes app_data;
	uint8_t flags = 0;
	app_data << Bytes(&flags, 1) << Bytes(info, info_len) << Bytes(stamp, STAMP_LEN);
	dest.announce(app_data);
	R_DEBUG("Discovery: announced, %u bytes of app data", (unsigned)app_data.size());
}

static bool can_run() {
	return started && settings::discovery_enabled && lora && lora->online();
}

void begin(LoRaInterface* l) {
	lora = l;
	started = true;
	next_announce = millis() + BOOT_ANNOUNCE_DELAY_MS
		+ RNS::Cryptography::random(1).data()[0] * BOOT_ANNOUNCE_JITTER_STEP_MS;
}

void loop() {
	if (!can_run()) {
		work = WORK_NONE;
		return;
	}
	if (!dest) {
		dest = RNS::Destination(RNS::Transport::identity(), RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE,
			"rnstransport", "discovery.interface");
		dest.accepts_links(false);
		R_INFO("Discovery: destination %s", dest.hash().toHex().c_str());
	}

	switch (work) {
	case WORK_WORKBLOCK: workblock_step(); break;
	case WORK_SEARCH:    search_step(); break;
	default: break;
	}

	bool due = (int32_t)(millis() - next_announce) >= 0;
	if (work == WORK_NONE && (due || !have_info || millis() - last_info_check >= INFO_CHECK_INTERVAL_MS)) check_info();
	if (due && work == WORK_NONE && stamp_ready) send_announce();
}

AnnounceResult announce() {
	if (!can_run() || !dest) return ANNOUNCE_OFF;
	if (work == WORK_NONE) check_info();
	if (work == WORK_NONE && stamp_ready) {
		send_announce();
		return ANNOUNCE_SENT;
	}
	next_announce = millis();
	return ANNOUNCE_WAITING;
}

bool busy() { return work != WORK_NONE; }

const RNS::Destination& destination() { return dest; }

}

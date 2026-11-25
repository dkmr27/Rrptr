#include "status/Status.h"

#include <Arduino.h>
#include <stdio.h>

#include <microReticulum/Identity.h>
#include <microReticulum/Transport.h>
#include <microReticulum/Utilities/Memory.h>

#include "board/Board.h"

namespace status {

// Reading the pool walks all its blocks, so not too often
static const uint32_t SAMPLE_INTERVAL_MS = 5000;

static char start_error_text[48] = "";
static size_t lowest_pool = 0;
static size_t lowest_heap = 0;
static bool sampled = false;
static uint32_t last_sample = 0;
static MemSnapshot low;
static bool have_low = false;

void set_start_error(const char* what) {
	snprintf(start_error_text, sizeof(start_error_text), "%s", what);
}

const char* start_error() { return start_error_text[0] ? start_error_text : nullptr; }

size_t pool_size() { return RNS::Utilities::Memory::heap_pool_size(); }
size_t pool_free() { return RNS::Utilities::Memory::heap_pool_free(); }
size_t heap_free() { return RNS::Utilities::Memory::heap_available(); }

static uint16_t count(size_t n) { return n > 0xFFFF ? 0xFFFF : (uint16_t)n; }

void take_snapshot(MemSnapshot& s) {
	bool pool = pool_size() != 0;
	s.uptime_s = millis() / 1000;
	s.free_bytes = pool ? pool_free() : heap_free();
	s.fragmented = pool ? RNS::Utilities::Memory::heap_pool_fragmented() : 0;
	s.paths = count(RNS::Transport::new_path_table().size());
	s.destinations = count(RNS::Identity::known_destinations().size());
	s.announces = count(RNS::Transport::announce_table().size());
	s.held_announces = count(RNS::Transport::held_announces().size());
	s.links = count(RNS::Transport::active_links().size());
	s.link_table = count(RNS::Transport::link_table().size());
	s.reverse_table = count(RNS::Transport::reverse_table().size());
	s.receipts = count(RNS::Transport::receipts().size());
	s.path_requests = count(RNS::Transport::path_requests().size());
	s.discovery_path_requests = count(RNS::Transport::discovery_path_requests().size());
	s.path_states = count(RNS::Transport::path_states().size());
	s.hashlist = count(RNS::Transport::packet_hashlist().size());
}

const MemSnapshot* low_snapshot() { return have_low ? &low : nullptr; }

void describe(const MemSnapshot& s, void (*line)(const char*)) {
	char text[160];
	uint32_t up = s.uptime_s;
	if (pool_size()) {
		snprintf(text, sizeof(text), "after %lud %02lu:%02lu:%02lu, pool free %lu (%u%% fragmented)",
			(unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
			(unsigned long)(up / 60 % 60), (unsigned long)(up % 60),
			(unsigned long)s.free_bytes, (unsigned)s.fragmented);
	} else {
		snprintf(text, sizeof(text), "after %lud %02lu:%02lu:%02lu, heap free %lu",
			(unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
			(unsigned long)(up / 60 % 60), (unsigned long)(up % 60), (unsigned long)s.free_bytes);
	}
	line(text);
	snprintf(text, sizeof(text), "paths %u, destinations %u, announces %u (held %u), links %u",
		s.paths, s.destinations, s.announces, s.held_announces, s.links);
	line(text);
	snprintf(text, sizeof(text),
		"link table %u, reverse table %u, receipts %u, path requests %u, discovery %u, path states %u, hashlist %u",
		s.link_table, s.reverse_table, s.receipts, s.path_requests, s.discovery_path_requests,
		s.path_states, s.hashlist);
	line(text);
}

static void take_sample() {
	last_sample = millis();
	size_t pool = pool_free();
	size_t heap = heap_free();
	if (!sampled || pool < lowest_pool) lowest_pool = pool;
	if (!sampled || heap < lowest_heap) lowest_heap = heap;
	sampled = true;
	board.note_pool(pool, lowest_pool);
	// A new low: keep what the tables held at that moment
	size_t watched = pool_size() ? pool : heap;
	if (!have_low || watched < low.free_bytes) {
		take_snapshot(low);
		have_low = true;
		board.note_snapshot(low);
	}
}

size_t pool_lowest() { take_sample(); return lowest_pool; }
size_t heap_lowest() { take_sample(); return lowest_heap; }

void sample() {
	if (sampled && millis() - last_sample < SAMPLE_INTERVAL_MS) return;
	take_sample();
}

}

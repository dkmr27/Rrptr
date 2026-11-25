// Status: health the node can report for itself, in stats, the banner and the
// NomadNet admin page: whether start-up finished, and how much RAM is left.
//
// Running out of RAM loading microReticulum's stores otherwise leaves a node that
// half works, with only a line in the boot log to say so.

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace status {
	// Start-up stopped early with this error; null if it finished
	void set_start_error(const char* what);
	const char* start_error();

	// nRF52 boards give microReticulum's tables a fixed pool, allocated once, so the
	// heap does not move as they fill; short-lived work like packets and crypto uses
	// the heap. Other boards have no pool and pool_size() is 0.
	size_t pool_size();
	size_t pool_free();
	size_t heap_free();
	// Lowest of each since boot, including now. Sampled every few seconds from
	// loop(), so a brief dip between samples is missed.
	size_t pool_lowest();
	size_t heap_lowest();
	void sample();

	// The free memory and the size of microReticulum's tables at one moment. One
	// is kept from the sample with the least free (the pool on nRF52, the heap on
	// other boards), so it shows what filled memory; nRF52 boards also keep one
	// in the crash record.
	struct MemSnapshot {
		uint32_t uptime_s;
		uint32_t free_bytes;
		uint32_t fragmented;  // % of the free pool outside its largest block
		uint16_t paths, destinations, announces, held_announces;
		uint16_t links, link_table, reverse_table, receipts;
		uint16_t path_requests, discovery_path_requests, path_states, hashlist;
	};
	// Fills s from the tables now. Reads counts only, so it allocates nothing.
	void take_snapshot(MemSnapshot& s);
	// The one with the least free since boot; null before the first sample
	const MemSnapshot* low_snapshot();
	// As three lines of text, each passed to line
	void describe(const MemSnapshot& s, void (*line)(const char*));
}

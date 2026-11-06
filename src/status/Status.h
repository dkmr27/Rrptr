// Status: health the node can report for itself, in stats, the banner and the
// NomadNet admin page: whether start-up finished, and how much RAM is left.
//
// Running out of RAM loading microReticulum's stores otherwise leaves a node that
// half works, with only a line in the boot log to say so.

#pragma once

#include <stddef.h>

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
}

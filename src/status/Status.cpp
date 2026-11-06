#include "status/Status.h"

#include <Arduino.h>
#include <stdio.h>

#include <microReticulum/Utilities/Memory.h>

namespace status {

// Reading the pool walks all its blocks, so not too often
static const uint32_t SAMPLE_INTERVAL_MS = 5000;

static char start_error_text[48] = "";
static size_t lowest_pool = 0;
static size_t lowest_heap = 0;
static bool sampled = false;
static uint32_t last_sample = 0;

void set_start_error(const char* what) {
	snprintf(start_error_text, sizeof(start_error_text), "%s", what);
}

const char* start_error() { return start_error_text[0] ? start_error_text : nullptr; }

size_t pool_size() { return RNS::Utilities::Memory::heap_pool_size(); }
size_t pool_free() { return RNS::Utilities::Memory::heap_pool_free(); }
size_t heap_free() { return RNS::Utilities::Memory::heap_available(); }

static void take_sample() {
	last_sample = millis();
	size_t pool = pool_free();
	size_t heap = heap_free();
	if (!sampled || pool < lowest_pool) lowest_pool = pool;
	if (!sampled || heap < lowest_heap) lowest_heap = heap;
	sampled = true;
}

size_t pool_lowest() { take_sample(); return lowest_pool; }
size_t heap_lowest() { take_sample(); return lowest_heap; }

void sample() {
	if (sampled && millis() - last_sample < SAMPLE_INTERVAL_MS) return;
	take_sample();
}

}

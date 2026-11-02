#include "log/Log.h"

#include <Arduino.h>
#include <stdarg.h>

namespace rlog {

#if defined(RRPTR_DEBUG_LOG)
static const RNS::LogLevel max_level = RNS::LOG_DEBUG;
static Mode current = BOTH;
#else
static const RNS::LogLevel max_level = RNS::LOG_INFO;
static Mode current = OFF;
#endif

static void print_line(const char* source, RNS::LogLevel level, const char* msg) {
	// Writing to a USB serial port with no host attached just drops the data
	if (!Serial) return;
	Serial.printf("%s [%s] %s: %s\r\n", RNS::getTimeString(), RNS::getLevelName(level), source, msg);
}

static void on_library_log(const char* msg, RNS::LogLevel level) {
	if (current == BOTH) print_line("uR", level, msg);
}

void begin() {
	RNS::loglevel(max_level);
	RNS::set_log_callback(on_library_log);
#if defined(RRPTR_DEBUG_LOG)
	// Give a serial monitor time to attach so the start-up messages are seen
	uint32_t start = millis();
	while (!Serial && millis() - start < 5000) delay(10);
	delay(100);
#endif
}

void set_mode(Mode mode) { current = mode; }
Mode mode() { return current; }

bool debug_build() {
#if defined(RRPTR_DEBUG_LOG)
	return true;
#else
	return false;
#endif
}

const char* mode_name(Mode mode) {
	switch (mode) {
	case ON:   return "on";
	case BOTH: return "both";
	default:   return "off";
	}
}

void write(RNS::LogLevel level, const char* fmt, ...) {
	if (current == OFF || level > max_level) return;
	char buf[192];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	print_line("rrptr", level, buf);
}

}

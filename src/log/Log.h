// Log: serial log output, switched by the `log` command.
//
//   off   nothing (the default after every boot, so the CLI stays readable)
//   on    Rrptr's own messages
//   both  Rrptr's messages and microReticulum's
//
// Rrptr code logs with R_ERROR / R_WARNING / R_INFO / R_DEBUG. microReticulum's
// own output is routed here through its log callback.
//
// A build with -DRRPTR_DEBUG_LOG starts in `both` at DEBUG level and waits up
// to 5 s at boot for a serial monitor, so start-up messages are seen. It needs
// -DRNS_LOG_LEVEL=RNS_LOG_LEVEL_DEBUG so microReticulum's debug messages are
// compiled in. Development builds only.

#pragma once

#include <microReticulum/Log.h>

#if defined(RRPTR_DEBUG_LOG) && RNS_LOG_LEVEL < RNS_LOG_LEVEL_DEBUG
	#warning "RRPTR_DEBUG_LOG without RNS_LOG_LEVEL_DEBUG: microReticulum's debug messages are not compiled in"
#endif

namespace rlog {
	enum Mode : uint8_t { OFF = 0, ON = 1, BOTH = 2 };

	// Call after Serial.begin()
	void begin();
	void set_mode(Mode mode);
	Mode mode();
	const char* mode_name(Mode mode);
	// True in a build with RRPTR_DEBUG_LOG
	bool debug_build();

	void write(RNS::LogLevel level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
}

#define R_ERROR(...)   rlog::write(RNS::LOG_ERROR, __VA_ARGS__)
#define R_WARNING(...) rlog::write(RNS::LOG_WARNING, __VA_ARGS__)
#define R_INFO(...)    rlog::write(RNS::LOG_INFO, __VA_ARGS__)
#define R_DEBUG(...)   rlog::write(RNS::LOG_DEBUG, __VA_ARGS__)

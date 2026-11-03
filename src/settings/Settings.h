// Settings: Rrptr's own provisioning namespaces, on top of microReticulum's
// built-in ones (General Config, Transport Config, Info, Metrics).
//
// The provisioning engine stores each namespace as a file on the flash
// filesystem and fires the setters below when a value is committed (live
// fields) or loaded at boot. The CLI and remote /provision both go through it.
//
// Namespace ids are permanent once shipped: append fields, never renumber.

#pragma once

#include <stdint.h>
#include <string>

#include "radio/RadioBand.h"

class Radio;

// Default radio settings for a blank node. Frequency 0 means "not configured":
// the radio stays off until `set radio`. A build can supply its own defaults
// with -DRRPTR_DEFAULT_FREQ_HZ=... (and the others), e.g. in platformio.local.ini.
#ifndef RRPTR_DEFAULT_FREQ_HZ
	#define RRPTR_DEFAULT_FREQ_HZ 0
#endif
#ifndef RRPTR_DEFAULT_BW_HZ
	#define RRPTR_DEFAULT_BW_HZ 125000
#endif
#ifndef RRPTR_DEFAULT_SF
	#define RRPTR_DEFAULT_SF 9
#endif
#ifndef RRPTR_DEFAULT_CR
	#define RRPTR_DEFAULT_CR 5
#endif
#ifndef RRPTR_DEFAULT_TX_DBM
	#define RRPTR_DEFAULT_TX_DBM 10
#endif
#ifndef RRPTR_DEFAULT_PREAMBLE
	#define RRPTR_DEFAULT_PREAMBLE 18
#endif

namespace settings {

	// ─── Namespace and field ids ────────────────────────────
	namespace node {
		constexpr uint16_t Id = 200;
		constexpr uint16_t Name      = 1;
		constexpr uint16_t LedMode   = 2;
		// 3: retired (Idle Sleep); do not reuse
		// next-id: 4
	}
	namespace radio {
		constexpr uint16_t Id0 = 201;   // radio 0; 202 reserved for radio 1
		constexpr uint16_t Frequency = 1;
		constexpr uint16_t Bandwidth = 2;
		constexpr uint16_t SpreadingFactor = 3;
		constexpr uint16_t CodingRate = 4;
		constexpr uint16_t TxPower = 5;
		constexpr uint16_t AgcResetMinutes = 6;
		constexpr uint16_t Preamble = 7;
		constexpr uint16_t RxBoostedGain = 8;
		// next-id: 9
	}
	namespace power {
		constexpr uint16_t Id = 210;
		constexpr uint16_t BatteryCalibration = 1;
		constexpr uint16_t Battery = 2;            // read-only, mV
		// next-id: 3
	}

	enum LedMode : uint8_t { LED_OFF = 0, LED_TX = 1 };

	// ─── Current values ─────────────────────────────────────
	// The node's name, or empty to use the default from node_name()
	extern std::string name;
	extern uint8_t led_mode;
	// Radio 0 settings; freq_hz 0 = not configured
	extern RadioConfig radio0;
	extern uint16_t agc_reset_minutes;
	extern float battery_calibration;

	// Name to show: the setting, or "Rrptr-" plus the last 4 hex digits of the
	// transport identity
	std::string node_name();

	// Registers the namespaces. Call after the filesystem is registered and
	// before Provisioner::begin() / Reticulum::start().
	void register_namespaces();

	// Radio 0 instance, so radio changes can be validated against the chip
	void attach_radio(Radio* radio);

	// Rewrites a stored TX power above the board's ceiling as the ceiling
	// (e.g. settings carried over from another board). Call once at boot after
	// the settings are loaded and the radio is attached.
	void clamp_stored_power();

	// True once after the radio settings changed; the main loop then applies
	// them (the provisioning engine wants setters to return quickly)
	bool take_radio_changed();
}

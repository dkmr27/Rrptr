#include "settings/Settings.h"

#include <microReticulum/Provisioning/Provisioning.h>
#include <microReticulum/Transport.h>
#include "log/Log.h"

#include "board/Board.h"
#include "radio/Radio.h"
#include "board/RadioPins.h"
#include "nomadnet/NomadNet.h"

using namespace RNS::Provisioning;

namespace settings {

std::string name;
uint8_t led_mode = LED_TX;
RadioConfig radio0 = {
	RRPTR_DEFAULT_FREQ_HZ, RRPTR_DEFAULT_BW_HZ, RRPTR_DEFAULT_SF, RRPTR_DEFAULT_CR,
	RRPTR_DEFAULT_TX_DBM, 0x12, RRPTR_DEFAULT_PREAMBLE, RADIO0_RX_BOOSTED_GAIN,
};
uint16_t agc_reset_minutes = 0;
uint8_t channel_access = CHANNEL_LBT;
float battery_calibration = 1.0f;
bool nomadnet_enabled = true;

static Radio* radio0_instance = nullptr;
static bool radio_changed = false;

std::string node_name() {
	if (!name.empty()) return name;
	std::string n = "Rrptr";
	if (RNS::Transport::identity()) {
		std::string hex = RNS::Transport::identity().hash().toHex();
		n += "-" + hex.substr(hex.size() - 4);
	}
	return n;
}

void attach_radio(Radio* radio) { radio0_instance = radio; }

bool take_radio_changed() {
	bool changed = radio_changed;
	radio_changed = false;
	return changed;
}

// Setter for one radio field: store it and let the main loop apply the whole
// config. Validation already happened in the commit hook.
static SetterFn radio_setter(void (*store)(fint_t)) {
	return [store](const Value& v) {
		store(v.as_int());
		radio_changed = true;
		return true;
	};
}

// Commit hook for the radio namespace: build the config the drafts would give
// and reject the whole change if the chip or board cannot do it, so a bad
// combination never reaches the radio. TX power above the board's ceiling is
// stored as the ceiling, so the saved value is the one actually used.
static void radio_commit_check(Namespace& ns) {
	RadioConfig c = radio0;
	Value v;
	if (ns.draft(radio::Frequency, v))       c.freq_hz = v.as_int();
	if (ns.draft(radio::Bandwidth, v))       c.bandwidth_hz = v.as_int();
	if (ns.draft(radio::SpreadingFactor, v)) c.sf = v.as_int();
	if (ns.draft(radio::CodingRate, v))      c.cr = v.as_int();
	if (ns.draft(radio::TxPower, v))         c.power_dbm = v.as_int();
	if (ns.draft(radio::Preamble, v))        c.preamble_len = v.as_int();

	if (c.freq_hz == 0) return;  // radio off: nothing to check
	if (radio0_instance && !radio0_instance->config_supported(c)) {
		R_WARNING("Settings: radio %lu Hz, %lu Hz BW, SF%u, CR%u, %d dBm not supported, change rejected",
			(unsigned long)c.freq_hz, (unsigned long)c.bandwidth_hz, c.sf, c.cr, c.power_dbm);
		ns.clear_draft();
		return;
	}
	// A frequency change can lower the ceiling too, so check even without a power draft
	int8_t ceiling = radio0_instance ? radio0_instance->max_output_power(c.freq_hz) : c.power_dbm;
	if (c.power_dbm > ceiling) ns.set_draft(radio::TxPower, Value((int)ceiling));
}

void clamp_stored_power() {
	if (!radio0_instance || radio0.freq_hz == 0) return;
	int8_t ceiling = radio0_instance->max_output_power(radio0.freq_hz);
	if (radio0.power_dbm <= ceiling) return;
	R_INFO("Settings: stored TX power %d dBm is above this board's %d dBm, saving %d dBm",
		radio0.power_dbm, ceiling, ceiling);
	Provisioner& p = Provisioner::instance();
	p.field(radio::Id0, radio::TxPower, Value((int)ceiling));
	p.commit(radio::Id0);
}

void register_namespaces() {
	Provisioner& p = Provisioner::instance();

	p.register_namespace("Rrptr Node", node::Id)
		.field_string("Name", node::Name, FF_LIVE_APPLY, "", 32,
			[](const Value& v) { name = v.as_string(); return true; },
			[]() { return name; })
		.field_enum("LED Mode", node::LedMode, FF_LIVE_APPLY, LED_TX,
			{ LED_OFF, LED_TX }, { "off", "tx" },
			[](const Value& v) { led_mode = v.as_int(); board.set_tx_led(led_mode == LED_TX); return true; },
			[]() { return (fenum_t)led_mode; })
		.end();

	p.register_namespace("Rrptr Radio 0", radio::Id0)
		.field_int("Frequency", radio::Frequency, FF_LIVE_APPLY, RRPTR_DEFAULT_FREQ_HZ, 0, 3000000000LL,
			radio_setter([](fint_t x) { radio0.freq_hz = x; }),
			[]() { return (fint_t)radio0.freq_hz; })
		.field_int("Bandwidth", radio::Bandwidth, FF_LIVE_APPLY, RRPTR_DEFAULT_BW_HZ, 7800, 1625000,
			radio_setter([](fint_t x) { radio0.bandwidth_hz = x; }),
			[]() { return (fint_t)radio0.bandwidth_hz; })
		.field_int("Spreading Factor", radio::SpreadingFactor, FF_LIVE_APPLY, RRPTR_DEFAULT_SF, 5, 12,
			radio_setter([](fint_t x) { radio0.sf = x; }),
			[]() { return (fint_t)radio0.sf; })
		.field_int("Coding Rate", radio::CodingRate, FF_LIVE_APPLY, RRPTR_DEFAULT_CR, 5, 8,
			radio_setter([](fint_t x) { radio0.cr = x; }),
			[]() { return (fint_t)radio0.cr; })
		.field_int("TX Power", radio::TxPower, FF_LIVE_APPLY, RRPTR_DEFAULT_TX_DBM, -18, 30,
			radio_setter([](fint_t x) { radio0.power_dbm = x; }),
			[]() { return (fint_t)radio0.power_dbm; })
		.field_int("AGC Reset Minutes", radio::AgcResetMinutes, FF_LIVE_APPLY, 0, 0, 1440,
			[](const Value& v) { agc_reset_minutes = v.as_int(); return true; },
			[]() { return (fint_t)agc_reset_minutes; })
		.field_int("Preamble", radio::Preamble, FF_LIVE_APPLY, RRPTR_DEFAULT_PREAMBLE, 6, 64,
			radio_setter([](fint_t x) { radio0.preamble_len = x; }),
			[]() { return (fint_t)radio0.preamble_len; })
		.field_bool("RX Boosted Gain", radio::RxBoostedGain, FF_LIVE_APPLY, RADIO0_RX_BOOSTED_GAIN,
			[](const Value& v) { radio0.rx_boosted_gain = v.as_bool(); radio_changed = true; return true; },
			[]() { return radio0.rx_boosted_gain; })
		.field_enum("Channel Access", radio::ChannelAccess, FF_LIVE_APPLY, CHANNEL_LBT,
			{ CHANNEL_LBT, CHANNEL_CAD }, { "lbt", "cad" },
			[](const Value& v) { channel_access = v.as_int(); return true; },
			[]() { return (fenum_t)channel_access; })
		.on_commit(radio_commit_check)
		.end();

	p.register_namespace("Rrptr Power", power::Id)
		.field_float("Battery Calibration", power::BatteryCalibration, FF_LIVE_APPLY, 1.0, 0.8, 1.2,
			[](const Value& v) { battery_calibration = v.as_float(); board.set_battery_calibration(battery_calibration); return true; },
			[]() { return (ffloat_t)battery_calibration; })
		.metric_int("Battery", power::Battery,
			[]() { return (fint_t)board.battery_mv(); })
		.end();

#if RRPTR_NOMADNET
	p.register_namespace("Rrptr NomadNet", nomad::Id)
		.field_bool("Enabled", nomad::Enabled, FF_REBOOT_REQUIRED, nomadnet_enabled,
			[](const Value& v) { nomadnet_enabled = v.as_bool(); return true; },
			[]() { return nomadnet_enabled; })
		.metric_bytes("Destination", nomad::Destination,
			[]() { return nomadnet::destination() ? nomadnet::destination().hash() : RNS::Bytes{}; })
		.end();
#endif
}

}

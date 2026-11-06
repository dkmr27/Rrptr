// NomadNet: the node's own small set of pages, served over a Reticulum link to
// any NomadNet client. The destination is nomadnetwork.node on the transport
// identity and its announces carry the node name.
//
// Pages: a front page, general statistics for anyone, and admin statistics, heard
// neighbours and controls (announces, reboot) for identities on the remote
// management list.
//
// In by default; build with -DRRPTR_NOMADNET=0 to leave it out. A build that has
// it can still turn it off with the Enabled setting (takes effect after a reboot).

#pragma once

#ifndef RRPTR_NOMADNET
	#define RRPTR_NOMADNET 1
#endif

#if RRPTR_NOMADNET
	#include <microReticulum/Transport.h>
	#include <microReticulum/Destination.h>
#endif

class Radio;
class LoRaInterface;

namespace nomadnet {
#if RRPTR_NOMADNET
	// Call once microReticulum has started; the radio and interface are reported on.
	// Does nothing if the setting has it turned off.
	void begin(Radio* radio, LoRaInterface* lora);
	// Announces about 30 s after boot and every 2 hours; call from loop()
	void loop();
	// Announces now. False if there is nothing to announce.
	bool announce();
	const RNS::Destination& destination();
#else
	inline void begin(Radio*, LoRaInterface*) {}
	inline void loop() {}
	inline bool announce() { return false; }
#endif
}

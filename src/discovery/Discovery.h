// Discovery: announces this node's radio interface so Reticulum nodes with
// discover_interfaces on can list it. Announce only: Rrptr does not listen for
// other nodes' discovery announces.
//
// The announce is on rnstransport.discovery.interface. Its app data is one flags
// byte, the interface info in msgpack, then a 32-byte proof-of-work stamp over the
// info. A stamp never expires, so it is kept in flash and only found again when
// the info changes (name, radio settings, location or firmware version).
//
// Off by default: it uses airtime and publishes the node's details. Announces go
// out about 30 s after boot, then every 6 hours.

#pragma once

#include <microReticulum/Transport.h>
#include <microReticulum/Destination.h>

class LoRaInterface;

namespace discovery {
	enum AnnounceResult {
		ANNOUNCE_OFF,       // turned off in the settings, or the radio is off
		ANNOUNCE_SENT,
		ANNOUNCE_WAITING,   // goes out once the stamp has been found
	};

	void begin(LoRaInterface* lora);
	// Does a slice of stamp work and sends announces when due; call from loop()
	void loop();
	AnnounceResult announce();
	// Finding a stamp: the main loop should not sleep meanwhile
	bool busy();
	const RNS::Destination& destination();
}

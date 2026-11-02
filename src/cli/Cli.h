// Cli: line-based serial command interface for bring-up and status.
//
// Commands follow MeshCore's style (e.g. `set radio 915.0,250,11,5`). Every
// `set` writes the same provisioning field that remote /provision uses, so the
// two can never disagree. Type `help` for the list.

#pragma once

class Radio;
class LoRaInterface;

namespace cli {
	// The radio and LoRa interface the status commands report on (may be null)
	void begin(Radio* radio, LoRaInterface* lora);
	// Reads and runs whatever has arrived on the serial port; call from loop()
	void poll();
}

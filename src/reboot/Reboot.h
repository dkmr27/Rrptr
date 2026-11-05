// Reboot: a planned restart from the CLI or remote provisioning. It closes the
// active links first, because nothing else tells a client that this node has
// forgotten a link: the client would keep using it until it times out, which can
// take minutes. A power cut or crash cannot do this.
//
// A request only sets a flag. A reply to it, such as the acknowledgement to the
// web console, is sent after the request returns and goes out first.

#pragma once

class Radio;
class LoRaInterface;

namespace reboot {
	void request();
	// Does the work once requested; call from loop()
	void check(Radio* radio, LoRaInterface* lora);
}

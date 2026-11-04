// Display: a small status screen on boards with an OLED (BOARD_HAS_DISPLAY).
// Boards without one compile to nothing. The panel keeps its image by itself,
// so it is only written when the text changes and light sleep is unaffected.
// The screen goes off 30 s after boot or the last press of the board's button.

#pragma once

#include "board_config.h"

class Radio;
class LoRaInterface;

namespace display {
#if defined(BOARD_HAS_DISPLAY) && BOARD_HAS_DISPLAY
	// The radio and LoRa interface to report on (may be null)
	void begin(Radio* radio, LoRaInterface* lora);
	// Redraws if anything shown has changed; call from loop()
	void update();
#else
	inline void begin(Radio*, LoRaInterface*) {}
	inline void update() {}
#endif
}

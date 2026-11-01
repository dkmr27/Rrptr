// Rrptr: Reticulum transport node for LoRa.
//
// Board support stage: brings up the board and reports version, reset reason
// and battery voltage every 5 s, flashing the LED with each report.

#include <Arduino.h>
#if defined(RRPTR_MCU_NRF52)
#include <Adafruit_TinyUSB.h>
#endif

#include "board/Board.h"

static void report() {
	Serial.printf("Rrptr %s (%s, %s) on %s, reset: %s, battery %u mV\r\n",
		RRPTR_VERSION, RRPTR_GIT_HASH, RRPTR_BUILD_DATE, board.name(),
		board.reset_reason(), board.battery_mv());
}

void setup() {
	board.begin();
	Serial.begin(115200);
}

void loop() {
	static uint32_t last = 0;
	if (millis() - last >= 5000) {
		last = millis();
		board.led(true);
		report();
		board.idle(50);
		board.led(false);
	}
	board.idle(10);
}

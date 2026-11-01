// Rrptr: Reticulum transport node for LoRa.
//
// Bootstrap stage: prints the version banner so the build setup can be checked.

#include <Arduino.h>
#if defined(RRPTR_MCU_NRF52)
#include <Adafruit_TinyUSB.h>
#endif

#include "board_config.h"

static void print_version() {
	Serial.printf("Rrptr %s (%s, %s) on %s\r\n",
		RRPTR_VERSION, RRPTR_GIT_HASH, RRPTR_BUILD_DATE, RRPTR_BOARD_NAME);
}

void setup() {
	Serial.begin(115200);
}

void loop() {
	static uint32_t last = 0;
	if (millis() - last >= 5000) {
		last = millis();
		print_version();
	}
	delay(10);
}

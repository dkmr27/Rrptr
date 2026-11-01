// Board: the hardware services the rest of Rrptr uses.
//
// Common behaviour (LED, rail, button) is implemented here from board_config.h;
// each MCU family (NRF52Board, later ESP32Board) supplies the rest. A variant
// with special needs can subclass its MCU board and define RRPTR_CUSTOM_BOARD.

#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "board/RadioPins.h"

class Board {
public:
	virtual ~Board() {}

	// Rail on, LED and button pins set up, ADC configured
	virtual void begin();
	const char* name() const { return BOARD_NAME; }

	// SPI bus for radio `index`, with its pins already assigned
	virtual SPIClass& radio_spi(uint8_t index) = 0;

	// Battery voltage in mV, 0 if the board cannot measure it
	virtual uint16_t battery_mv() = 0;

	void led(bool on);
	bool button_pressed();

	// Sleep until an event or `max_ms` has passed
	virtual void idle(uint32_t max_ms) = 0;

	virtual void reboot() = 0;
	// Reboot into the bootloader's OTA update mode, if supported
	virtual bool enter_ota() { return false; }
	virtual const char* reset_reason() = 0;
};

extern Board& board;

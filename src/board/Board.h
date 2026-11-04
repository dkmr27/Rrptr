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

	// SPI bus for radio `index`, started and on the radio's pins
	virtual SPIClass& radio_spi(uint8_t index) = 0;

	// Battery voltage in mV, 0 if the board cannot measure it
	virtual uint16_t battery_mv() = 0;
	// Per-unit trim of the divider ratio, from the Battery Calibration setting
	void set_battery_calibration(float factor) { _vbat_calibration = factor; }

	void led(bool on);
	// LED for transmit activity; does nothing while the LED mode is off
	void tx_led(bool on) { if (_tx_led_enabled || !on) led(on); }
	void set_tx_led(bool enabled) { _tx_led_enabled = enabled; if (!enabled) led(false); }
	bool button_pressed();

	// Sleep until wake() is called or `max_ms` has passed
	virtual void idle(uint32_t max_ms) = 0;
	// Ends the current or next idle() early; safe to call from an interrupt.
	// Not virtual: on ESP32 it runs from IRAM while the flash cache may be off,
	// so it must not go through a vtable in flash. Each MCU file defines it.
	void wake();
	// Console activity: keep the console responsive for a while. Used by boards
	// whose idle() sleeps deeply enough to stop it.
	virtual void stay_awake() {}

	virtual void reboot() = 0;
	// Reboot into the bootloader's OTA update mode, if supported
	virtual bool enter_ota() { return false; }
	virtual const char* reset_reason() = 0;

protected:
	float _vbat_calibration = 1.0f;
	bool  _tx_led_enabled = true;
};

extern Board& board;

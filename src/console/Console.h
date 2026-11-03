// Console: the serial port the CLI and logs use. Output goes to USB serial
// while a host has it open, and to the board's UART pins if it has them
// (BOARD_UART_RX/TX); input is read from both. The UART is useful when USB
// must stay unplugged, e.g. for current measurements.
//
// The UART has no way to tell whether anything is listening, so it always
// gets the output. Writes to it wait for the bytes to go out at 115200 baud.

#pragma once

#include <Arduino.h>

class Console : public Print {
public:
	void begin(unsigned long baud);
	// A host has the USB port open
	bool usb_connected();
	bool has_uart() const;
	// Switches the UART off until reboot (its receiver keeps the high-speed
	// clock running, about 0.9 mA on nRF52)
	void end_uart();

	int available();
	int read();
	void flush();

	size_t write(uint8_t c) override;
	size_t write(const uint8_t* buf, size_t len) override;

private:
	bool _uart_on = false;
};

extern Console console;

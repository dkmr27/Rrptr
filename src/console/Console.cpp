#include "console/Console.h"

#include "board_config.h"

#ifndef BOARD_UART_RX
	#define BOARD_UART_RX -1
	#define BOARD_UART_TX -1
#endif

#define CONSOLE_UART (BOARD_UART_RX >= 0 && BOARD_UART_TX >= 0)

Console console;

void Console::begin(unsigned long baud) {
	Serial.begin(baud);
#if CONSOLE_UART
	#if defined(RRPTR_MCU_ESP32)
	Serial1.begin(baud, SERIAL_8N1, BOARD_UART_RX, BOARD_UART_TX);
	#else
	Serial1.setPins(BOARD_UART_RX, BOARD_UART_TX);
	Serial1.begin(baud);
	#endif
	_uart_on = true;
#endif
}

bool Console::usb_connected() { return (bool)Serial; }

bool Console::has_uart() const { return _uart_on; }

void Console::end_uart() {
#if CONSOLE_UART
	if (!_uart_on) return;
	Serial1.flush();
	Serial1.end();
	_uart_on = false;
#endif
}

int Console::available() {
	int n = Serial.available();
#if CONSOLE_UART
	if (_uart_on) n += Serial1.available();
#endif
	return n;
}

int Console::read() {
	if (Serial.available()) return Serial.read();
#if CONSOLE_UART
	if (_uart_on && Serial1.available()) return Serial1.read();
#endif
	return -1;
}

void Console::flush() {
	Serial.flush();
#if CONSOLE_UART
	if (_uart_on) Serial1.flush();
#endif
}

size_t Console::write(uint8_t c) { return write(&c, 1); }

size_t Console::write(const uint8_t* buf, size_t len) {
	// Writing to a USB serial port with no host attached just drops the data
	if (Serial) Serial.write(buf, len);
#if CONSOLE_UART
	if (_uart_on) Serial1.write(buf, len);
#endif
	return len;
}

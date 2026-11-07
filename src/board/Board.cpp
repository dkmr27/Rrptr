#include "board/Board.h"

#ifndef BOARD_RAIL_ACTIVE_HIGH
	#define BOARD_RAIL_ACTIVE_HIGH 1
#endif

void Board::begin() {
#if BOARD_RAIL_EN >= 0
	pinMode(BOARD_RAIL_EN, OUTPUT);
	digitalWrite(BOARD_RAIL_EN, BOARD_RAIL_ACTIVE_HIGH ? HIGH : LOW);
#endif
#if BOARD_LED >= 0
	pinMode(BOARD_LED, OUTPUT);
	led(false);
#endif
#if BOARD_BUTTON >= 0
	pinMode(BOARD_BUTTON, INPUT_PULLUP);
#endif
}

void Board::led(bool on) {
#if BOARD_LED >= 0
	digitalWrite(BOARD_LED, (on == (bool)BOARD_LED_ACTIVE_HIGH) ? HIGH : LOW);
#else
	(void)on;
#endif
}

bool Board::button_pressed() {
#if BOARD_BUTTON >= 0
	return digitalRead(BOARD_BUTTON) == LOW;
#else
	return false;
#endif
}

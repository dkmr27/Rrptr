#pragma once

#if defined(RRPTR_MCU_ESP32)

#include "board/Board.h"

class ESP32Board : public Board {
public:
	void begin() override;
	SPIClass& radio_spi(uint8_t index) override;
	uint16_t battery_mv() override;
	void idle(uint32_t max_ms) override;
	void reboot() override;
	const char* reset_reason() override;
};

#endif

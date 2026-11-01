#pragma once

#if defined(RRPTR_MCU_NRF52)

#include "board/Board.h"

class NRF52Board : public Board {
public:
	void begin() override;
	SPIClass& radio_spi(uint8_t index) override;
	uint16_t battery_mv() override;
	void idle(uint32_t max_ms) override;
	void reboot() override;
	bool enter_ota() override;
	const char* reset_reason() override;
};

#endif

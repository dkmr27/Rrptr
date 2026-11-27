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
	void watchdog_begin(uint32_t seconds) override;
	void watchdog_feed() override;
	bool has_ota() const override { return true; }
	bool enter_ota() override;
	const char* const* ota_help() const override;
	const char* reset_reason() override;
	const char* last_crash() override;
	const char* crash_stack() override;
	bool clear_crash() override;
	void note_pool(size_t free, size_t lowest) override;
	void note_snapshot(const status::MemSnapshot& s) override;
	const status::MemSnapshot* crash_snapshot() override;
	const char* bootloader_version() override;
};

#endif

#if defined(RRPTR_MCU_ESP32)

#include "board/esp32/ESP32Board.h"

#include <esp_system.h>

// The radio gets its own SPI host; the default SPI (FSPI) is left free
static SPIClass radio0_spi(HSPI);

// Given by wake(), taken by idle(). A wake that comes before idle() is kept,
// so an interrupt just before the loop goes idle is not missed.
static SemaphoreHandle_t wake_sem = nullptr;

#if !defined(RRPTR_CUSTOM_BOARD)
static ESP32Board esp32_board;
Board& board = esp32_board;
#endif

void ESP32Board::begin() {
	Board::begin();
	wake_sem = xSemaphoreCreateBinary();
}

SPIClass& ESP32Board::radio_spi(uint8_t index) {
	(void)index;
	// A bus handed to RadioLib must already be running (see NRF52Board)
	static bool started = false;
	if (!started) {
		radio0_spi.begin(RADIO0_SCK, RADIO0_MISO, RADIO0_MOSI);
		started = true;
	}
	return radio0_spi;
}

uint16_t ESP32Board::battery_mv() {
#if BOARD_VBAT >= 0
	// analogReadMilliVolts() applies the chip's factory ADC calibration
	uint32_t sum = 0;
	for (int i = 0; i < 4; i++) sum += analogReadMilliVolts(BOARD_VBAT);
	return (uint16_t)((sum / 4.0f) * BOARD_VBAT_DIVIDER * _vbat_calibration + 0.5f);
#else
	return 0;
#endif
}

void ESP32Board::idle(uint32_t max_ms) {
	if (!wake_sem) { delay(max_ms); return; }
	xSemaphoreTake(wake_sem, pdMS_TO_TICKS(max_ms));
}

// Called from the radio interrupt, which runs from IRAM
void IRAM_ATTR Board::wake() {
	if (!wake_sem) return;
	if (xPortInIsrContext()) {
		BaseType_t woken = pdFALSE;
		xSemaphoreGiveFromISR(wake_sem, &woken);
		if (woken) portYIELD_FROM_ISR();
	} else {
		xSemaphoreGive(wake_sem);
	}
}

void ESP32Board::reboot() {
	esp_restart();
}

const char* ESP32Board::reset_reason() {
	switch (esp_reset_reason()) {
	case ESP_RST_POWERON:   return "power on";
	case ESP_RST_EXT:       return "reset pin";
	case ESP_RST_SW:        return "software";
	case ESP_RST_PANIC:     return "panic";
	case ESP_RST_INT_WDT:
	case ESP_RST_TASK_WDT:
	case ESP_RST_WDT:       return "watchdog";
	case ESP_RST_DEEPSLEEP: return "wake from sleep";
	case ESP_RST_BROWNOUT:  return "brownout";
	default:                return "other";
	}
}

#endif

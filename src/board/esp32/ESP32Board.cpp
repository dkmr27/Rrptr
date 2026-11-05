#if defined(RRPTR_MCU_ESP32)

#include "board/esp32/ESP32Board.h"

#include <driver/gpio.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "console/Console.h"

// Time awake after boot, console input or a UART or button wake, so a USB host
// can enumerate and a terminal gets a responsive console
static const uint32_t STAY_AWAKE_MS = 60000;
// Shorter idles are retries with work waiting: not worth the sleep transition
static const uint32_t MIN_SLEEP_MS = 10;
// Longest wait while awake, so typing on the console stays responsive
static const uint32_t AWAKE_IDLE_MS = 50;

static uint32_t awake_until = 0;

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
	setCpuFrequencyMhz(80);
	stay_awake();
}

void ESP32Board::stay_awake() { awake_until = millis() + STAY_AWAKE_MS; }

// Sets the wake sources, sleeps and restores them. gpio_wakeup_enable() replaces
// the pin's interrupt type, so DIO1's interrupt is masked during this and its
// rising edge restored after; Radio::poll() also checks the pin's level, as an
// edge during sleep raises no interrupt. A level already active wakes at once,
// so nothing that happened just before sleeping is missed.
static void light_sleep(uint32_t max_ms) {
	console.flush();  // the UART stops during sleep

	gpio_intr_disable((gpio_num_t)RADIO0_DIO1);
	gpio_wakeup_enable((gpio_num_t)RADIO0_DIO1, GPIO_INTR_HIGH_LEVEL);
#if BOARD_BUTTON >= 0
	gpio_wakeup_enable((gpio_num_t)BOARD_BUTTON, GPIO_INTR_LOW_LEVEL);
#endif
#if BOARD_UART_RX >= 0
	// Wakes on the start bit; a pull-up stops an unconnected pin waking it.
	// The first characters are lost.
	bool uart = console.has_uart();
	if (uart) {
		gpio_pullup_en((gpio_num_t)BOARD_UART_RX);
		gpio_wakeup_enable((gpio_num_t)BOARD_UART_RX, GPIO_INTR_LOW_LEVEL);
	}
#endif
	esp_sleep_enable_gpio_wakeup();
	esp_sleep_enable_timer_wakeup((uint64_t)max_ms * 1000);

	esp_light_sleep_start();

	bool by_gpio = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
	bool radio = digitalRead(RADIO0_DIO1) == HIGH;

#if BOARD_UART_RX >= 0
	if (uart) gpio_wakeup_disable((gpio_num_t)BOARD_UART_RX);
#endif
#if BOARD_BUTTON >= 0
	gpio_wakeup_disable((gpio_num_t)BOARD_BUTTON);
#endif
	gpio_wakeup_disable((gpio_num_t)RADIO0_DIO1);
	gpio_set_intr_type((gpio_num_t)RADIO0_DIO1, GPIO_INTR_POSEDGE);
	gpio_intr_enable((gpio_num_t)RADIO0_DIO1);

	if (by_gpio && !radio) board.stay_awake();
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

	// USB stops during light sleep and can need a replug after it, so stay awake
	// while a host is attached. Its frames can go missing for a moment with the
	// cable in, so a short gap must not allow a sleep.
	if (HWCDC::isPlugged()) stay_awake();
	bool awake = (int32_t)(millis() - awake_until) < 0;
	if (awake || max_ms < MIN_SLEEP_MS) {
		xSemaphoreTake(wake_sem, pdMS_TO_TICKS(min(max_ms, AWAKE_IDLE_MS)));
		return;
	}
	if (xSemaphoreTake(wake_sem, 0) == pdTRUE) return;  // something is waiting
	light_sleep(max_ms);
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

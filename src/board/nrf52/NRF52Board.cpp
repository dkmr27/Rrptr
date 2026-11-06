#if defined(RRPTR_MCU_NRF52)

#include "board/nrf52/NRF52Board.h"

// The radio gets its own SPI peripheral; the default SPI (SPIM3) is left for
// the external flash. A second radio would take SPIM1 (shared with TWIM1, so
// only while Wire1 is unused).
static SPIClass radio0_spi(NRF_SPIM2, RADIO0_MISO, RADIO0_SCK, RADIO0_MOSI);

// Given by wake(), taken by idle(). A wake that comes before idle() is kept,
// so an interrupt just before the loop goes idle is not missed.
static SemaphoreHandle_t wake_sem = nullptr;

#if !defined(RRPTR_CUSTOM_BOARD)
static NRF52Board nrf52_board;
Board& board = nrf52_board;
#endif

void NRF52Board::begin() {
	Board::begin();
	wake_sem = xSemaphoreCreateBinary();

#if BOARD_HAS_EXT_FLASH
	// Keep the flash deselected until storage takes over
	pinMode(BOARD_FLASH_CS, OUTPUT);
	digitalWrite(BOARD_FLASH_CS, HIGH);
#endif

	// Battery ADC: 0.6 V reference with 1/6 gain = 0..3.6 V. The 1M/1M
	// divider has a high source resistance, so use the longest sample time
	// or the reading comes out low.
	analogReference(AR_INTERNAL);
	analogReadResolution(12);
	analogSampleTime(40);
}

SPIClass& NRF52Board::radio_spi(uint8_t index) {
	(void)index;
	// RadioLib only starts its own default SPI; a bus handed to it must already
	// be running, or the first transfer waits forever. begin() is a no-op once
	// the bus is up.
	radio0_spi.begin();
	return radio0_spi;
}

uint16_t NRF52Board::battery_mv() {
#if BOARD_VBAT >= 0
	uint32_t sum = 0;
	for (int i = 0; i < 4; i++) sum += analogRead(BOARD_VBAT);
	float adc_mv = (sum / 4.0f) * 3600.0f / 4096.0f;
	return (uint16_t)(adc_mv * BOARD_VBAT_DIVIDER * _vbat_calibration + 0.5f);
#else
	return 0;
#endif
}

void NRF52Board::idle(uint32_t max_ms) {
	// Blocking in FreeRTOS lets its idle task put the CPU to sleep
	if (!wake_sem) { delay(max_ms); return; }
	xSemaphoreTake(wake_sem, pdMS_TO_TICKS(max_ms));
}

void Board::wake() {
	if (!wake_sem) return;
	if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) {  // in an interrupt handler
		BaseType_t woken = pdFALSE;
		xSemaphoreGiveFromISR(wake_sem, &woken);
		portYIELD_FROM_ISR(woken);
	} else {
		xSemaphoreGive(wake_sem);
	}
}

void NRF52Board::reboot() {
	NVIC_SystemReset();
}

// The bootloader advertises as AdaDFU and takes the firmware over Bluetooth. The
// application needs no Bluetooth code for this: it leaves a value for the
// bootloader in GPREGRET and resets.
bool NRF52Board::enter_ota() {
	enterOTADfu();  // does not return
	return true;
}

const char* const* NRF52Board::ota_help() const {
	static const char* const HELP[] = {
		"This reboots into the bootloader, which advertises as AdaDFU over Bluetooth.",
		"The node stays off the air until it is updated or reset.",
		nullptr,
	};
	return HELP;
}

const char* NRF52Board::reset_reason() {
	uint32_t r = readResetReason();
	if (r & POWER_RESETREAS_RESETPIN_Msk) return "reset pin";
	if (r & POWER_RESETREAS_DOG_Msk)      return "watchdog";
	if (r & POWER_RESETREAS_SREQ_Msk)     return "software";
	if (r & POWER_RESETREAS_LOCKUP_Msk)   return "lockup";
	if (r & POWER_RESETREAS_OFF_Msk)      return "wake from off";
	if (r & POWER_RESETREAS_VBUS_Msk)     return "usb power";
	if (r == 0)                           return "power on";
	return "other";
}

// The UF2 bootloader holds its INFO_UF2.TXT in flash, which includes
// "UF2 Bootloader <version>". Where depends on the bootloader build, so search
// the known area, as MeshCore's NRF52Board does (MIT, by the MeshCore project and
// its contributors).
const char* NRF52Board::bootloader_version() {
	static const char MARKER[] = "UF2 Bootloader ";
	static const size_t MARKER_LEN = sizeof(MARKER) - 1;
	static const uint8_t* const AREA = (const uint8_t*)0x000FB000;
	static const size_t AREA_LEN = 0x3000;
	static char version[24];
	static bool searched = false;

	if (!searched) {
		searched = true;
		for (size_t i = 0; i + MARKER_LEN < AREA_LEN; i++) {
			if (memcmp(AREA + i, MARKER, MARKER_LEN)) continue;
			const uint8_t* text = AREA + i + MARKER_LEN;
			size_t room = AREA_LEN - i - MARKER_LEN;
			size_t n = 0;
			while (n < sizeof(version) - 1 && n < room && text[n] > ' ' && text[n] < 0x7f) {
				version[n] = text[n];
				n++;
			}
			version[n] = 0;
			break;
		}
	}
	return version[0] ? version : nullptr;
}

#endif

#if defined(RRPTR_MCU_NRF52)

#include "board/nrf52/NRF52Board.h"

#include <microReticulum/Utilities/Memory.h>

// The radio gets its own SPI peripheral; the default SPI (SPIM3) is left for
// the external flash. A second radio would take SPIM1 (shared with TWIM1, so
// only while Wire1 is unused). Boards whose radio is on the flash's pins set
// BOARD_RADIO_SHARES_FLASH_SPI: two peripherals cannot drive the same pins, so
// the radio uses the default SPI too, with its own chip select.
#if defined(BOARD_RADIO_SHARES_FLASH_SPI) && BOARD_RADIO_SHARES_FLASH_SPI
static SPIClass& radio0_spi = SPI;
#else
static SPIClass radio0_spi(NRF_SPIM2, RADIO0_MISO, RADIO0_SCK, RADIO0_MOSI);
#endif

// Given by wake(), taken by idle(). A wake that comes before idle() is kept,
// so an interrupt just before the loop goes idle is not missed.
static SemaphoreHandle_t wake_sem = nullptr;

#if !defined(RRPTR_CUSTOM_BOARD)
static NRF52Board nrf52_board;
Board& board = nrf52_board;
#endif

// ─── Crash record ───────────────────────────────────────────
// A crash normally ends in the core's HardFault handler, which just resets, so
// it reads as a software reset, the same as a planned reboot. The core's handler
// cannot be replaced (its definition is not weak), so begin() turns on the
// separate MemManage, BusFault and UsageFault exceptions, which catch bad
// pointers, bad instructions and traps. Their handler writes where the code was
// to a page of internal flash, so the next boot can report it. Only a fault
// those cannot take (one inside a fault handler, for example) still goes to
// HardFault, and reads as "software, unplanned".
// The PC and LR are looked up in the build's firmware.elf:
//   arm-none-eabi-addr2line -f -C -e firmware.elf <pc> <lr>
// The LR often only points into the function that crashed, so the handler also
// keeps the first few words on the stack above the fault that look like return
// addresses: odd (Thumb) and inside the firmware's code. Looked up the same way,
// they are the calls that led there, nearest first. Stale values left on the
// stack can look the same, so one that makes no sense as a caller is noise.
//
// The record is in flash, not RAM, as the bootloader runs on every reset and
// uses RAM after the application's start. The page is the last of the area
// the core sets aside for its internal filesystem (0xED000 to 0xF4000, below
// the bootloader), which Rrptr does not use: its storage is on external flash.
//
// microReticulum also resets the board itself when its memory pool runs out
// (Reticulum::loop() catches bad_alloc and calls NVIC_SystemReset). That is no
// fault, so the patch in scripts/patch_microreticulum.py sends it to
// rrptr_out_of_memory() below, which records it the same way.
//
// GPREGRET2 (GPREGRET is the bootloader's) says why the last reset happened:
// PLANNED_REBOOT from reboot(), CRASH_REBOOT from a handler. A software reset
// with neither was not Rrptr's doing.

static const uint32_t CRASH_PAGE = 0xF3000;
static const uint32_t CRASH_MAGIC = 0x52524332;  // "RRC2"; a record of an older layout reads as blank
static const uint8_t PLANNED_REBOOT = 0xA5;
static const uint8_t CRASH_REBOOT = 0x5C;

enum CrashKind : uint32_t { KIND_FAULT = 0, KIND_OUT_OF_MEMORY = 1 };

static const size_t CRASH_STACK_WORDS = 8;   // return addresses kept
static const size_t CRASH_STACK_SCAN = 256;  // stack words searched for them

struct CrashRecord {
	uint32_t magic;
	uint32_t build;        // end of the firmware's code, to tell another build's record
	uint32_t kind;         // CrashKind
	uint32_t count;        // crashes recorded since the page was blank
	uint32_t pc, lr, cfsr, hfsr, fault_addr;
	uint32_t uptime_s;
	uint32_t pool_free, pool_lowest;
	uint32_t stack[CRASH_STACK_WORDS];  // likely return addresses, nearest first; 0 = none
	uint32_t check;
};

extern "C" uint32_t __etext[];       // from the linker script: end of the code
extern "C" uint32_t __isr_vector[];  // from the startup code: start of the code
extern "C" uint32_t __StackTop[];    // from the linker script: end of RAM

static const CrashRecord& stored_crash = *(const CrashRecord*)CRASH_PAGE;
// Kept up to date by note_pool(), copied into the record by the handler
static volatile uint32_t last_pool_free = 0, last_pool_lowest = 0;

static uint32_t crash_check(const CrashRecord& r) {
	const uint32_t* w = (const uint32_t*)&r;
	uint32_t sum = 0x2B1F0E57;
	for (size_t i = 0; i < offsetof(CrashRecord, check) / 4; i++) sum = (sum << 5 | sum >> 27) ^ w[i];
	return sum;
}

static bool crash_valid() {
	return stored_crash.magic == CRASH_MAGIC && stored_crash.check == crash_check(stored_crash);
}

// Read once at boot, and cleared for the next one
static bool booted_from_crash = false;
static bool booted_from_planned = false;

static void read_reset_mark() {
	uint8_t mark = NRF_POWER->GPREGRET2;
	NRF_POWER->GPREGRET2 = 0;
	booted_from_planned = mark == PLANNED_REBOOT;
	booted_from_crash = mark == CRASH_REBOOT;
}

static void nvmc_wait() { while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} }

// Direct NVMC access, as the SoftDevice is not running
static void write_crash_record(const CrashRecord& r) {
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
	nvmc_wait();
	NRF_NVMC->ERASEPAGE = CRASH_PAGE;
	nvmc_wait();
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
	nvmc_wait();
	const uint32_t* src = (const uint32_t*)&r;
	volatile uint32_t* dst = (volatile uint32_t*)CRASH_PAGE;
	for (size_t i = 0; i < sizeof(r) / 4; i++) {
		dst[i] = src[i];
		nvmc_wait();
	}
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
	nvmc_wait();
}

// Fills r.stack from the stack above the fault's frame
static void scan_stack(CrashRecord& r, const uint32_t* sp) {
	size_t found = 0;
	for (size_t i = 0; i < CRASH_STACK_SCAN && found < CRASH_STACK_WORDS && sp + i < __StackTop; i++) {
		uint32_t v = sp[i];
		if ((v & 1) && v > (uint32_t)__isr_vector && v < (uint32_t)__etext) r.stack[found++] = v;
	}
	while (found < CRASH_STACK_WORDS) r.stack[found++] = 0;
}

// Called by the handler below with the stack frame the fault pushed
// (r0, r1, r2, r3, r12, lr, pc, xpsr, then the FPU registers if they were in
// use) and the handler's EXC_RETURN, which says whether they were
extern "C" __attribute__((used)) void rrptr_fault(const uint32_t* frame, uint32_t exc_return) {
	NRF_POWER->GPREGRET2 = CRASH_REBOOT;
	CrashRecord r;
	r.magic = CRASH_MAGIC;
	r.build = (uint32_t)__etext;
	r.kind = KIND_FAULT;
	r.count = (crash_valid() ? stored_crash.count : 0) + 1;
	r.lr = frame[5];
	r.pc = frame[6];
	r.cfsr = SCB->CFSR;
	r.hfsr = SCB->HFSR;
	r.fault_addr = (SCB->CFSR & SCB_CFSR_BFARVALID_Msk) ? SCB->BFAR
		: (SCB->CFSR & SCB_CFSR_MMARVALID_Msk) ? SCB->MMFAR : 0;
	r.uptime_s = millis() / 1000;
	r.pool_free = last_pool_free;
	r.pool_lowest = last_pool_lowest;
	// The caller's stack starts after the frame, and a word of padding if the
	// fault aligned it (xPSR bit 9)
	const uint32_t* caller = frame + ((exc_return & 0x10) ? 8 : 26) + ((frame[7] & (1 << 9)) ? 1 : 0);
	scan_stack(r, caller);
	r.check = crash_check(r);
	write_crash_record(r);
	NVIC_SystemReset();
}

// Finds the frame on whichever stack was in use: the task's (PSP) or, in an
// interrupt, the main stack (MSP)
#define RRPTR_FAULT_HANDLER(name)                      \
	extern "C" __attribute__((naked)) void name(void) { \
		__asm volatile(                                  \
			"tst lr, #4      \n"                         \
			"ite eq          \n"                         \
			"mrseq r0, msp   \n"                         \
			"mrsne r0, psp   \n"                         \
			"mov r1, lr      \n"                         \
			"b rrptr_fault   \n");                       \
	}
RRPTR_FAULT_HANDLER(MemoryManagement_Handler)
RRPTR_FAULT_HANDLER(BusFault_Handler)
RRPTR_FAULT_HANDLER(UsageFault_Handler)

// Called by the patched Reticulum::loop() in place of its reset when the pool
// runs out. The PC recorded is the call's return address, in Reticulum::loop().
extern "C" __attribute__((noinline, noreturn)) void rrptr_out_of_memory(void) {
	uint32_t pool = RNS::Utilities::Memory::heap_pool_free();
	CrashRecord r;
	r.magic = CRASH_MAGIC;
	r.build = (uint32_t)__etext;
	r.kind = KIND_OUT_OF_MEMORY;
	r.count = (crash_valid() ? stored_crash.count : 0) + 1;
	r.pc = (uint32_t)__builtin_return_address(0);
	r.lr = 0;
	r.cfsr = 0;
	r.hfsr = 0;
	r.fault_addr = 0;
	r.uptime_s = millis() / 1000;
	r.pool_free = pool;
	r.pool_lowest = (last_pool_lowest && last_pool_lowest < pool) ? last_pool_lowest : pool;
	// Nothing to scan: the stack was unwound by the time the loop caught it
	for (size_t i = 0; i < CRASH_STACK_WORDS; i++) r.stack[i] = 0;
	r.check = crash_check(r);
	Serial.flush();  // the log line before this was cut short by the reset
	__disable_irq();
	write_crash_record(r);
	NRF_POWER->GPREGRET2 = CRASH_REBOOT;
	NVIC_SystemReset();
	while (true) {}
}

void NRF52Board::note_pool(size_t free, size_t lowest) {
	last_pool_free = free;
	last_pool_lowest = lowest;
}

// Erasing the page leaves it blank, which reads as no record. The reset reason
// still says crash until the next boot.
static bool crash_cleared = false;

bool NRF52Board::clear_crash() {
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
	nvmc_wait();
	NRF_NVMC->ERASEPAGE = CRASH_PAGE;
	nvmc_wait();
	NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
	nvmc_wait();
	crash_cleared = true;
	return true;
}

const char* NRF52Board::last_crash() {
	if (crash_cleared) return nullptr;
	if (!crash_valid()) return booted_from_crash ? "details not recorded" : nullptr;
	static char text[220];
	const CrashRecord& r = stored_crash;
	uint32_t up = r.uptime_s;
	if (r.kind == KIND_OUT_OF_MEMORY) {
		snprintf(text, sizeof(text),
			"out of memory at 0x%08lx, after %lud %02lu:%02lu:%02lu, pool free %lu, lowest %lu, crash %lu%s",
			(unsigned long)r.pc,
			(unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
			(unsigned long)(up / 60 % 60), (unsigned long)(up % 60),
			(unsigned long)r.pool_free, (unsigned long)r.pool_lowest, (unsigned long)r.count,
			r.build == (uint32_t)__etext ? "" : ", in other firmware");
		return text;
	}
	snprintf(text, sizeof(text),
		"PC 0x%08lx, LR 0x%08lx, CFSR 0x%08lx, HFSR 0x%08lx, address 0x%08lx, "
		"after %lud %02lu:%02lu:%02lu, pool free %lu, lowest %lu, crash %lu%s",
		(unsigned long)r.pc, (unsigned long)r.lr, (unsigned long)r.cfsr,
		(unsigned long)r.hfsr, (unsigned long)r.fault_addr,
		(unsigned long)(up / 86400), (unsigned long)(up / 3600 % 24),
		(unsigned long)(up / 60 % 60), (unsigned long)(up % 60),
		(unsigned long)r.pool_free, (unsigned long)r.pool_lowest, (unsigned long)r.count,
		r.build == (uint32_t)__etext ? "" : ", in other firmware");
	return text;
}

const char* NRF52Board::crash_stack() {
	if (crash_cleared || !crash_valid() || !stored_crash.stack[0]) return nullptr;
	static char text[8 + CRASH_STACK_WORDS * 11];
	size_t at = snprintf(text, sizeof(text), "stack");
	for (size_t i = 0; i < CRASH_STACK_WORDS && stored_crash.stack[i]; i++) {
		at += snprintf(text + at, sizeof(text) - at, " 0x%08lx", (unsigned long)stored_crash.stack[i]);
	}
	return text;
}

void NRF52Board::begin() {
	read_reset_mark();
	SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
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
	NRF_POWER->GPREGRET2 = PLANNED_REBOOT;
	NVIC_SystemReset();
}

// The bootloader advertises as AdaDFU and takes the firmware over Bluetooth. The
// application needs no Bluetooth code for this: it leaves a value for the
// bootloader in GPREGRET and resets.
bool NRF52Board::enter_ota() {
	NRF_POWER->GPREGRET2 = PLANNED_REBOOT;
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
	if (r & POWER_RESETREAS_SREQ_Msk) {
		if (booted_from_crash) {
			return crash_valid() && stored_crash.kind == KIND_OUT_OF_MEMORY ? "out of memory" : "crash";
		}
		if (booted_from_planned) return "software";
		// Not Rrptr: e.g. the first boot after flashing, or a USB 1200 baud touch
		return "software, unplanned";
	}
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

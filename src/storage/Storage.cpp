#include "storage/Storage.h"

#include <Arduino.h>
#include "log/Log.h"
#include <microReticulum/Transport.h>

#include "board_config.h"

#if defined(RRPTR_MCU_NRF52)
	#if !BOARD_HAS_EXT_FLASH
		#error "nRF52 boards need external flash: internal flash (28 KB) is too small for microReticulum"
	#endif
	#include <microStore/Adapters/FlashFSFileSystem.h>
#elif defined(RRPTR_MCU_ESP32)
	#include <LittleFS.h>
	#include <microStore/Adapters/PosixFileSystem.h>
#endif

static microStore::FileSystem filesystem;
static char description[32] = "none";
static const char* error = nullptr;

bool storage_begin() {
#if BOARD_HAS_EXT_FLASH
	// BOARD_FLASH_DEVICE names a chip definition from microStore (e.g. W25Q128).
	// The chip is on the board's default SPI bus.
	static const SPIFlash_Device_t device = BOARD_FLASH_DEVICE;
	filesystem = microStore::Adapters::FlashFSFileSystem(&device, BOARD_FLASH_CS);
	if (!filesystem.init()) {
		error = BOARD_FLASH_NAME " flash not found or not mountable, check its wiring and power";
		R_ERROR("Storage: %s", error);
		return false;
	}
	snprintf(description, sizeof(description), "%s %lu KB",
		BOARD_FLASH_NAME, (unsigned long)(device.total_size / 1024));

	// Far more room than the compiled-in defaults assume: keep a larger path table
	RNS::Transport::path_table_maxsize(500);
	RNS::Transport::path_store_segment_size(24576);
	RNS::Transport::path_store_segment_count(8);

	R_INFO("Storage: %s", description);
	return true;
#elif defined(RRPTR_MCU_ESP32)
	// LittleFS on the MCU's own flash, in the partition table's data partition.
	// The POSIX adapter mounts it at an empty base path, making it the default
	// file system, so microReticulum's relative paths ("./config") work; the
	// Arduino LittleFS API would need every path to start with "/".
	filesystem = microStore::Adapters::PosixFileSystem();
	if (!filesystem.init()) {
		error = "internal flash file system could not be mounted";
		R_ERROR("Storage: %s", error);
		return false;
	}
	snprintf(description, sizeof(description), "LittleFS %lu KB",
		(unsigned long)(LittleFS.totalBytes() / 1024));

	RNS::Transport::path_table_maxsize(500);
	RNS::Transport::path_store_segment_size(24576);
	RNS::Transport::path_store_segment_count(8);

	R_INFO("Storage: %s", description);
	return true;
#else
	error = "no storage configured for this board";
	return false;
#endif
}

void storage_trim_hashlist() {
	// About 250 entries of 33 bytes; a store at its normal size stays below this
	static const size_t INDEX_MAX = 8192;
	static const char* const INDEX = "./hashlist_store/index.dat";
	if (!filesystem.exists(INDEX)) return;
	size_t size = filesystem.size(INDEX);
	if (size <= INDEX_MAX) return;
	R_WARNING("Storage: packet hashlist index is %u bytes, too big to load, deleting it", (unsigned)size);
	static const char* const FILES[] = { "./hashlist_store/seg0.dat", "./hashlist_store/seg1.dat",
		"./hashlist_store/journal.dat", "./hashlist_store/compact.tmp", INDEX };
	for (const char* file : FILES) {
		if (filesystem.exists(file)) filesystem.remove(file);
	}
}

microStore::FileSystem& storage_filesystem() { return filesystem; }
const char* storage_description() { return description; }
const char* storage_error() { return error; }

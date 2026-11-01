// Storage: the flash filesystem that microReticulum and Rrptr's own settings
// live on, through microStore.
//
// nRF52 boards need external flash (BOARD_HAS_EXT_FLASH): the internal
// filesystem is only 28 KB, too small for microReticulum's tables. There is no
// fallback; if the chip does not mount, the node does not start.

#pragma once

#include <microStore/FileSystem.h>

// Mounts the filesystem; call once before anything reads or writes files.
// Returns false if it could not be mounted, with the reason in storage_error().
bool storage_begin();

microStore::FileSystem& storage_filesystem();

// Where the files live, for status output, e.g. "W25Q128 16384 KB"
const char* storage_description();
// Why storage_begin() failed, or nullptr
const char* storage_error();

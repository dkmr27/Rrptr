// The nRF52 core's HardwareSerial declares these virtuals but never defines
// them (Uart overrides them all). Its typeinfo is only emitted where the first
// of them is defined, so with RTTI on (microReticulum needs it) any use of
// Serial1 fails to link with "undefined reference to typeinfo for
// HardwareSerial". Defining them here emits it.

#if defined(RRPTR_MCU_NRF52)

#include <HardwareSerial.h>

void HardwareSerial::begin(unsigned long) {}
void HardwareSerial::begin(unsigned long, uint16_t) {}
void HardwareSerial::end() {}
int HardwareSerial::availableForWrite() { return 0; }

#endif

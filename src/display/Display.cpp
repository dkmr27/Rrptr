#include "display/Display.h"

#if defined(BOARD_HAS_DISPLAY) && BOARD_HAS_DISPLAY

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <string.h>

#include "board/Board.h"
#include "log/Log.h"
#include "lora/LoRaInterface.h"
#include "radio/Radio.h"
#include "settings/Settings.h"

namespace display {

static const uint32_t CHECK_INTERVAL_MS = 1000;
static const int PROBE_ATTEMPTS = 5;
// Time the screen stays on after boot or a button press
static const uint32_t SCREEN_ON_MS = 30000;

static Adafruit_SSD1306 oled(BOARD_OLED_WIDTH, BOARD_OLED_HEIGHT, &Wire, BOARD_OLED_RESET);
static bool present = false;
static Radio* radio = nullptr;
static LoRaInterface* lora = nullptr;
static char shown[128] = "";
static bool screen_on = true;
static uint32_t screen_on_at = 0;

// Resets the panel (if the board can) and checks that it answers. Returns the
// Wire error of the last attempt, 0 if it answered. The T3S3's panel often does
// not answer the first attempt, so a single try leaves the display off.
static uint8_t probe() {
	uint8_t error = 0;
	for (int attempt = 1; attempt <= PROBE_ATTEMPTS; attempt++) {
#if BOARD_OLED_RESET >= 0
		pinMode(BOARD_OLED_RESET, OUTPUT);
		digitalWrite(BOARD_OLED_RESET, LOW);
		delay(20);
		digitalWrite(BOARD_OLED_RESET, HIGH);
#endif
		delay(50);
		Wire.beginTransmission(BOARD_OLED_ADDRESS);
		error = Wire.endTransmission();
		R_DEBUG("Display: probe %d: error %u", attempt, error);
		if (error == 0) break;
	}
	return error;
}

void begin(Radio* r, LoRaInterface* l) {
	radio = r;
	lora = l;
	Wire.begin(BOARD_OLED_SDA, BOARD_OLED_SCL);
	uint8_t error = probe();
	if (error != 0) {
		R_WARNING("Display: no OLED found at 0x%02x (error %u)", BOARD_OLED_ADDRESS, error);
		return;
	}
	if (!oled.begin(SSD1306_SWITCHCAPVCC, BOARD_OLED_ADDRESS, false, false)) {
		R_WARNING("Display: could not start the OLED");
		return;
	}
	oled.setTextColor(SSD1306_WHITE);
	oled.setTextSize(1);
	present = true;
	screen_on_at = millis();
	update();
}

// Display off and charge pump off is the panel's lowest-power state; it keeps
// its image and text updates still reach it while off
static void set_screen(bool on) {
	if (on == screen_on) return;
	screen_on = on;
	if (on) {
		oled.ssd1306_command(SSD1306_CHARGEPUMP);
		oled.ssd1306_command(0x14);
		oled.ssd1306_command(SSD1306_DISPLAYON);
	} else {
		oled.ssd1306_command(SSD1306_DISPLAYOFF);
		oled.ssd1306_command(SSD1306_CHARGEPUMP);
		oled.ssd1306_command(0x10);
	}
}

// The screen's text, one line per row
static void format(char* buf, size_t len) {
	int n = snprintf(buf, len, "%s\n", settings::node_name().c_str());
	const RadioConfig& c = settings::radio0;
	if (!radio) {
		snprintf(buf + n, len - n, "\nradio init failed");
	} else if (c.freq_hz == 0) {
		snprintf(buf + n, len - n, "\nradio not configured");
	} else {
		snprintf(buf + n, len - n, "\n%.3f MHz\nBW %g kHz\nSF%u CR4/%u\nTX %d dBm\n%s",
			c.freq_hz / 1e6, c.bandwidth_hz / 1000.0, c.sf, c.cr, radio->applied_power_dbm(),
			lora && lora->online() ? "" : "radio off");
	}
}

void update() {
	if (!present) return;

	// A press also wakes the board from light sleep, so it is seen here
	if (board.button_pressed()) {
		screen_on_at = millis();
		set_screen(true);
	} else if (screen_on && millis() - screen_on_at >= SCREEN_ON_MS) {
		set_screen(false);
	}

	static uint32_t last = 0;
	if (shown[0] && millis() - last < CHECK_INTERVAL_MS) return;
	last = millis();

	char text[sizeof(shown)];
	format(text, sizeof(text));
	if (!strcmp(text, shown)) return;
	strcpy(shown, text);

	oled.clearDisplay();
	oled.setCursor(0, 0);
	oled.print(text);
	oled.display();
}

}

#endif

// Rrptr OTA updater for ESP32: a small app of its own in the second app slot, so
// Rrptr itself needs no Wi-Fi code. Rrptr's `ota` command reboots into it.
//
// It starts an open Wi-Fi access point with one page for uploading Rrptr's
// firmware.bin, which it writes to the main slot and boots. If no upload starts in
// 10 minutes it goes back to Rrptr, so an abandoned update does not leave the node
// off the air. Once an upload has started the old firmware is gone, so it then
// stays here until one succeeds; if an upload stalls it restarts to accept another.
//
// Flash it once over USB, into the second slot, with the upload command in the
// variant's platformio.ini. Normal uploads of Rrptr leave it alone.

#include <Arduino.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_system.h>

#include "board_config.h"

static const uint32_t IDLE_TIMEOUT_MS = 10UL * 60 * 1000;
static const uint32_t UPLOAD_STALL_MS = 20000;

static const char PAGE[] =
	"<!doctype html><html><head><meta name=viewport content='width=device-width'><title>Rrptr update</title></head>"
	"<body style='font-family:sans-serif;margin:2em'><h1>Rrptr update</h1>"
	"<p>Choose the board's <b>firmware.bin</b>. Keep this page open until it finishes.</p>"
	"<form method=post action=/update enctype=multipart/form-data>"
	"<p><input type=file name=firmware accept=.bin required></p><p><input type=submit value=Update></p>"
	"</form></body></html>";

static WebServer server(80);
static bool upload_started = false;
// Set in the main loop, read by the stall watchdog
static volatile bool upload_running = false;
static volatile uint32_t last_upload_data = 0;

static void set_led(bool on) {
#if BOARD_LED >= 0
	pinMode(BOARD_LED, OUTPUT);
	digitalWrite(BOARD_LED, on == (BOARD_LED_ACTIVE_HIGH != 0) ? HIGH : LOW);
#endif
}

// If the client's Wi-Fi drops, the socket never closes and the web server waits for
// its next byte forever with no timeout, holding up loop(). So this runs as a task
// of its own. Restarting returns here, as the boot setting has not changed, and a
// partly written image cannot boot.
static void stall_watchdog(void*) {
	for (;;) {
		vTaskDelay(pdMS_TO_TICKS(1000));
		if (upload_running && millis() - last_upload_data >= UPLOAD_STALL_MS) {
			Serial.println("Updater: upload stalled, restarting to accept another");
			delay(100);
			esp_restart();
		}
	}
}

static void handle_upload() {
	HTTPUpload& upload = server.upload();
	last_upload_data = millis();
	switch (upload.status) {
	case UPLOAD_FILE_START:
		upload_started = true;
		upload_running = true;
		Serial.printf("Updater: receiving %s\r\n", upload.filename.c_str());
		// The next slot after this one is Rrptr's
		if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
		break;
	case UPLOAD_FILE_WRITE:
		if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial);
		break;
	case UPLOAD_FILE_END:
		// Checks the image, and sets it to boot if it is good
		upload_running = false;
		if (Update.end(true)) Serial.printf("Updater: wrote %u bytes\r\n", upload.totalSize);
		else Update.printError(Serial);
		break;
	case UPLOAD_FILE_ABORTED:
		upload_running = false;
		Update.abort();
		Serial.println("Updater: upload aborted");
		break;
	default:
		break;
	}
}

static void handle_update_done() {
	if (Update.hasError() || !Update.isFinished()) {
		String message = String("<!doctype html><body style='font-family:sans-serif;margin:2em'><h1>Update failed</h1><p>")
			+ Update.errorString() + "</p><p><a href=/>Try again</a></p></body>";
		server.send(500, "text/html", message);
		return;
	}
	server.send(200, "text/html",
		"<!doctype html><body style='font-family:sans-serif;margin:2em'><h1>Update done</h1><p>Rebooting into Rrptr.</p></body>");
	delay(1000);
	esp_restart();
}

void setup() {
	Serial.begin(115200);
	set_led(true);
	xTaskCreate(stall_watchdog, "stall", 2048, nullptr, 1, nullptr);

	uint8_t mac[6];
	esp_efuse_mac_get_default(mac);
	char ssid[24];
	snprintf(ssid, sizeof(ssid), "Rrptr-update-%02X%02X", mac[4], mac[5]);

	WiFi.mode(WIFI_AP);
	WiFi.softAP(ssid);
	server.on("/", HTTP_GET, []() { server.send(200, "text/html", PAGE); });
	server.on("/update", HTTP_POST, handle_update_done, handle_upload);
	server.begin();
	Serial.printf("Updater: join Wi-Fi %s, then open http://%s\r\n", ssid, WiFi.softAPIP().toString().c_str());
}

void loop() {
	server.handleClient();
	delay(2);

	static uint32_t last_retry = 0;
	if (upload_started || millis() < IDLE_TIMEOUT_MS || millis() - last_retry < 10000) return;
	last_retry = millis();
	const esp_partition_t* main_app = esp_ota_get_next_update_partition(nullptr);
	if (main_app && esp_ota_set_boot_partition(main_app) == ESP_OK) {
		Serial.println("Updater: no upload, going back to Rrptr");
		delay(100);
		esp_restart();
	}
	Serial.println("Updater: no valid firmware to go back to, waiting for an upload");
}

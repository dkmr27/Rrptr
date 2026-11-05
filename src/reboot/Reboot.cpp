#include "reboot/Reboot.h"

#include <Arduino.h>
#include <set>

#include <microReticulum/Link.h>
#include <microReticulum/Transport.h>

#include "board/Board.h"
#include "console/Console.h"
#include "log/Log.h"
#include "lora/LoRaInterface.h"
#include "radio/Radio.h"

namespace reboot {

static const uint32_t MIN_WAIT_MS = 1000;
static const uint32_t MAX_WAIT_MS = 10000;     // in case the radio is stuck
// What fits in the transmit queue beside a reply; any others time out as before
static const int MAX_LINKS_CLOSED = LoRaInterface::TX_QUEUE_MAX - 2;

static bool requested = false;
static bool links_closed = false;
static bool into_ota = false;
static uint32_t requested_at = 0;

void request() {
	if (requested) return;
	requested = true;
	requested_at = millis();
}

void request_ota() {
	request();
	into_ota = true;
}

static void close_links() {
	// A copy, as closing a link removes it from the set
	std::set<RNS::Link> links(RNS::Transport::active_links());
	int closed = 0;
	for (const RNS::Link& link : links) {
		if (closed == MAX_LINKS_CLOSED) break;
		try {
			const_cast<RNS::Link&>(link).teardown();
			closed++;
		}
		catch (const std::exception& e) {
			R_WARNING("Reboot: could not close a link: %s", e.what());
		}
	}
	R_INFO("Reboot: closed %d of %u links", closed, (unsigned)links.size());
}

void check(Radio* radio, LoRaInterface* lora) {
	if (!requested) return;
	if (!links_closed) {
		links_closed = true;
		if (lora && lora->online()) close_links();
	}
	uint32_t waited = millis() - requested_at;
	bool sent = !lora || !radio || (lora->queued() == 0 && !radio->transmitting());
	if ((waited >= MIN_WAIT_MS && sent) || waited >= MAX_WAIT_MS) {
		R_INFO(into_ota ? "Rebooting into OTA update mode" : "Rebooting");
		console.flush();
		if (into_ota) board.enter_ota();
		board.reboot();
	}
}

}

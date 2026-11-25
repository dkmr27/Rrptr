# Fixes in microReticulum that are not yet upstream, applied to the env's copy of
# the library before it is built. Each fix replaces an exact piece of the source;
# if that text has changed, as after a library update, the build stops so the
# fix can be checked rather than silently lost. Remove a fix once upstream has it.

import os

Import("env")

MARKER = "Rrptr patch"

# Transport::jobs() pumps the links' resource watchdogs while _jobs_running is set.
# A watchdog that resends (an unanswered resource advertisement, a part request
# re-sent) goes through Transport::outbound(), which waits for _jobs_running to
# clear: with one thread it never does, and the node hangs. The pump now runs once
# jobs are done, as the announce retransmissions already are.
TRANSPORT_FIXES = [
	(
		"\tint count;\n\t_jobs_running = true;\n",
		"\tint count;\n"
		"\tstd::vector<Link> tick_links;  // Rrptr patch: pumped after the jobs\n"
		"\t_jobs_running = true;\n",
	),
	(
		"\t\t\t\tfor (auto& link_const : active_links) {\n"
		"\t\t\t\t\tLink& link = const_cast<Link&>(link_const);\n"
		"\t\t\t\t\tif (link.status() != Type::Link::CLOSED) {\n"
		"\t\t\t\t\t\tlink.tick_resources();\n"
		"\t\t\t\t\t}\n"
		"\t\t\t\t}\n",
		"\t\t\t\tfor (auto& link_const : active_links) {\n"
		"\t\t\t\t\tif (link_const.status() != Type::Link::CLOSED) {\n"
		"\t\t\t\t\t\ttick_links.push_back(link_const);  // Rrptr patch\n"
		"\t\t\t\t\t}\n"
		"\t\t\t\t}\n",
	),
	(
		"\t_jobs_running = false;\n\n\t// CBA send announce retransmission packets\n",
		"\t_jobs_running = false;\n\n"
		"\t// Rrptr patch: resource watchdogs may send, so they run once jobs are done\n"
		"\tfor (auto& link : tick_links) {\n"
		"\t\ttry {\n"
		"\t\t\tif (link.status() != Type::Link::CLOSED) link.tick_resources();\n"
		"\t\t}\n"
		"\t\tcatch (const std::exception& e) {\n"
		"\t\t\tERRORF(\"Error while pumping link resources: %s\", e.what());\n"
		"\t\t}\n"
		"\t}\n\n"
		"\t// CBA send announce retransmission packets\n",
	),
]


# Reticulum::loop() resets the board when the pool runs out. On nRF52 that reset
# now goes through Rrptr's rrptr_out_of_memory(), which records it as a crash
# first (src/board/nrf52/NRF52Board.cpp).
RETICULUM_FIXES = [
	(
		"using namespace RNS;\n",
		"using namespace RNS;\n\n"
		"#if defined(RRPTR_MCU_NRF52)\n"
		"extern \"C\" void rrptr_out_of_memory(void);  // Rrptr patch: records the reset\n"
		"#endif\n",
	),
	(
		"\t\tERROR(\"Reticulum::loop: bad_alloc - OUT OF MEMORY\");\n"
		"\t\t// Critical OOM, restarting\n"
		"#if defined(ESP32)\n",
		"\t\tERROR(\"Reticulum::loop: bad_alloc - OUT OF MEMORY\");\n"
		"\t\t// Critical OOM, restarting\n"
		"#if defined(RRPTR_MCU_NRF52)\n"
		"\t\trrptr_out_of_memory();  // Rrptr patch\n"
		"#elif defined(ESP32)\n",
	),
]


# Interface::handle_incoming() and send_outgoing() also reset the board when the
# pool runs out, each with its own NVIC_SystemReset(). Those resets read as
# "software, unplanned" with no record, so they go the same way. The PC
# recorded is in the function that caught it.
def interface_oom(function):
	error = "\t\tERROR(\"Interface::%s: bad_alloc - OUT OF MEMORY\");\n" % function
	return (
		error + "\t\t// Critical OOM, restarting\n#if defined(ESP32)\n",
		error + "\t\t// Critical OOM, restarting\n"
		"#if defined(RRPTR_MCU_NRF52)\n"
		"\t\trrptr_out_of_memory();  // Rrptr patch\n"
		"#elif defined(ESP32)\n",
	)

INTERFACE_FIXES = [
	RETICULUM_FIXES[0],  # the declaration
	interface_oom("send_outgoing"),
	interface_oom("handle_incoming"),
]


def patch(path, fixes):
	with open(path, newline="", encoding="utf-8") as f:
		text = f.read()
	if MARKER in text:
		return
	newline = "\r\n" if "\r\n" in text else "\n"
	text = text.replace("\r\n", "\n")
	for old, new in fixes:
		if text.count(old) != 1:
			raise SystemExit("Rrptr: %s has changed, its patch in %s needs checking"
				% (path, os.path.basename(__file__)))
		text = text.replace(old, new)
	with open(path, "w", newline="", encoding="utf-8") as f:
		f.write(text.replace("\n", newline))
	print("Rrptr: patched " + path)


libdeps = env.subst("$PROJECT_LIBDEPS_DIR/$PIOENV")
transport = os.path.join(libdeps, "microReticulum", "src", "microReticulum", "Transport.cpp")
if os.path.exists(transport):
	patch(transport, TRANSPORT_FIXES)
reticulum = os.path.join(libdeps, "microReticulum", "src", "microReticulum", "Reticulum.cpp")
if os.path.exists(reticulum):
	patch(reticulum, RETICULUM_FIXES)
interface = os.path.join(libdeps, "microReticulum", "src", "microReticulum", "Interface.cpp")
if os.path.exists(interface):
	patch(interface, INTERFACE_FIXES)

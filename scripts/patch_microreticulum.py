# Fixes in microReticulum that are not yet upstream, applied to the env's copy of
# the library before it is built. Each fix replaces an exact piece of the source;
# if that text has changed, as after a library update, the build stops so the
# fix can be checked rather than silently lost. Remove a fix once upstream has it.

import os

Import("env")

TRANSPORT_FIXES = [
	# The path and known destinations stores get their record limit only after
	# init() has loaded them, so the prune init() does at boot, which also
	# rewrites the index so evicted keys stay gone, sees no limit. A put()
	# evicts only from the index in RAM, and Rrptr's stores compact only when
	# full, so the index on flash keeps every key since the last compaction:
	# each boot loaded all of them (184 destinations against a limit of 100)
	# until the next new one arrived. The limit is now set before init().
	(
		"\t\t\tTRACE(\"Initializing path table store...\");\n",
		"\t\t\t_path_store.set_max_recs(_path_table_maxsize);  // Rrptr patch: before init() prunes\n"
		"\t\t\tTRACE(\"Initializing path table store...\");\n",
	),
	(
		"\t\t\tTRACE(\"Initializing known destinations store...\");\n",
		"\t\t\tIdentity::_known_store.set_max_recs(Identity::_known_destinations_maxsize);  // Rrptr patch: before init() prunes\n"
		"\t\t\tTRACE(\"Initializing known destinations store...\");\n",
	),

	# jobs() sends a neighbour probe when a link proof times out, and from its
	# periodic neighbour scan, while _jobs_running is set. Sending goes through
	# outbound(), which waits for _jobs_running to clear: with one thread it never
	# does, and the node hangs, as with the resource watchdogs before upstream
	# #103. A probe asked for during jobs() is now queued and sent once it is done.
	(
		"DestinationEntry empty_destination_entry;\n",
		"DestinationEntry empty_destination_entry;\n\n"
		"// Rrptr patch: neighbour probes asked for while jobs() runs, sent after it\n"
		"static std::vector<Bytes> rrptr_deferred_probes;\n",
	),
	(
		"\tTRACEF(\"Probing neighbor %s\", neighbor_hash.toHex().c_str());\n",
		"\tTRACEF(\"Probing neighbor %s\", neighbor_hash.toHex().c_str());\n"
		"\tif (_jobs_running) {  // Rrptr patch: sending now would wait on jobs() forever\n"
		"\t\tif (rrptr_deferred_probes.size() < 8 && std::find(rrptr_deferred_probes.begin(),\n"
		"\t\t\t\trrptr_deferred_probes.end(), neighbor_hash) == rrptr_deferred_probes.end()) {\n"
		"\t\t\trrptr_deferred_probes.push_back(neighbor_hash);\n"
		"\t\t}\n"
		"\t\treturn true;\n"
		"\t}\n",
	),
	(
		"\t// CBA send announce retransmission packets\n",
		"#if RNS_NEIGHBOR_PROBING\n"
		"\t// Rrptr patch: the neighbour probes asked for during jobs\n"
		"\tif (!rrptr_deferred_probes.empty()) {\n"
		"\t\tstd::vector<Bytes> probes;\n"
		"\t\tprobes.swap(rrptr_deferred_probes);\n"
		"\t\tfor (const Bytes& neighbor_hash : probes) {\n"
		"\t\t\ttry {\n"
		"\t\t\t\t_dispatch_neighbor_probe(neighbor_hash);\n"
		"\t\t\t}\n"
		"\t\t\tcatch (const std::exception& e) {\n"
		"\t\t\t\tERRORF(\"Error while sending a neighbor probe: %s\", e.what());\n"
		"\t\t\t}\n"
		"\t\t}\n"
		"\t}\n"
		"#endif\n\n"
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


# Each fix is checked on its own, so one added later still reaches a library
# copy that already has the others
def patch(path, fixes):
	with open(path, newline="", encoding="utf-8") as f:
		text = f.read()
	newline = "\r\n" if "\r\n" in text else "\n"
	text = text.replace("\r\n", "\n")
	changed = False
	for old, new in fixes:
		if new in text:
			continue
		if text.count(old) != 1:
			raise SystemExit("Rrptr: %s has changed, its patch in %s needs checking"
				% (path, os.path.basename(__file__)))
		text = text.replace(old, new)
		changed = True
	if not changed:
		return
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

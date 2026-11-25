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

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

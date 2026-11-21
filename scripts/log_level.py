# Sets microReticulum's compiled-in log level from the one development switch,
# -DRRPTR_DEBUG_LOG, so it never has to be given twice:
#   with it     RNS_LOG_LEVEL_DEBUG (its debug messages compiled in)
#   without it  RNS_LOG_LEVEL_INFO
# The switch is looked for in build_flags (including platformio.local.ini) and
# in the PLATFORMIO_BUILD_FLAGS environment variable.

import os

Import("env")

flags = " ".join(env.GetProjectOption("build_flags", [])).split()
flags += os.environ.get("PLATFORMIO_BUILD_FLAGS", "").split()
debug = "-DRRPTR_DEBUG_LOG" in flags

env.Append(CPPDEFINES=[
    ("RNS_LOG_LEVEL", "RNS_LOG_LEVEL_DEBUG" if debug else "RNS_LOG_LEVEL_INFO"),
])

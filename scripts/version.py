# Stamps the firmware with its version, git hash and build date.
#
# Defines RRPTR_VERSION (from [rrptr] version in platformio.ini),
# RRPTR_GIT_HASH (short hash, "-dirty" with uncommitted changes) and
# RRPTR_BUILD_DATE (UTC, YYYY-MM-DD).

import datetime
import subprocess

Import("env")


def git(*args):
    try:
        return subprocess.check_output(
            ["git", *args], cwd=env.subst("$PROJECT_DIR"), stderr=subprocess.DEVNULL
        ).decode().strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


version = env.GetProjectConfig().get("rrptr", "version")
git_hash = git("rev-parse", "--short=7", "HEAD") or "unknown"
if git_hash != "unknown" and git("status", "--porcelain", "--untracked-files=no"):
    git_hash += "-dirty"
build_date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d")

env.Append(CPPDEFINES=[
    ("RRPTR_VERSION", env.StringifyMacro(version)),
    ("RRPTR_GIT_HASH", env.StringifyMacro(git_hash)),
    ("RRPTR_BUILD_DATE", env.StringifyMacro(build_date)),
])

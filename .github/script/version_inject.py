Import("env")
import subprocess
import os
import re

def git_cmd(args):
    """Run git command with safe.directory workaround for Docker environments."""
    return subprocess.check_output(
        ["git", "-c", "safe.directory=" + os.getcwd()] + args,
        stderr=subprocess.DEVNULL
    ).decode().strip()

def get_version():
    try:
        tag = git_cmd(["describe", "--tags", "--exact-match", "HEAD"])
        m = re.match(r"^v(\d+\.\d+\.\d+)$", tag)
        if m:
            return m.group(1)
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    try:
        short_hash = git_cmd(["rev-parse", "--short=8", "HEAD"])
        return "0.0.0-dev-" + short_hash
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass
    return "0.0.0-unknown"

version = get_version()
print("Firmware version: " + version)
env.Append(CPPDEFINES=[("FW_VERSION", env.StringifyMacro(version))])

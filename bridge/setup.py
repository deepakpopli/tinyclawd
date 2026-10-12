#!/usr/bin/env python3
"""one command to hook tiny clawd into claude code on linux:
  python3 setup.py           service + hooks + status line usage
  python3 setup.py --remove  undo all three
every file it changes gets a backup next to it first."""
import shutil
import subprocess
import sys
import time
from pathlib import Path

import install_hooks

HERE = Path(__file__).resolve().parent
PY = "/usr/bin/python3" if Path("/usr/bin/python3").exists() else sys.executable
UNIT = Path.home() / ".config/systemd/user/tinyclawd-daemon.service"
USAGE = 'printf "%%s" "$input" | %s %s usage >/dev/null 2>&1 &' % (PY, HERE / "tinyclawd.py")


def backup(p):
    b = p.with_name(p.name + ".bak-tinyclawd-" + time.strftime("%Y%m%d-%H%M%S"))
    shutil.copy(p, b)


def service(remove):
    ctl = ["systemctl", "--user"]
    if remove:
        subprocess.run(ctl + ["disable", "--now", "tinyclawd-daemon"], capture_output=True)
        UNIT.unlink(missing_ok=True)
        subprocess.run(ctl + ["daemon-reload"], capture_output=True)
        return "service removed"
    UNIT.parent.mkdir(parents=True, exist_ok=True)
    text = (HERE / "tinyclawd-daemon.service").read_text()
    UNIT.write_text(text.replace("@DIR@", str(HERE)).replace("@PY@", PY))
    subprocess.run(ctl + ["daemon-reload"], capture_output=True)
    r = subprocess.run(ctl + ["enable", "--now", "tinyclawd-daemon"], capture_output=True, text=True)
    return "service running" if r.returncode == 0 else "service failed: " + r.stderr.strip()


def status_line(remove):
    """adds the usage line to the status line script, right after it reads stdin"""
    import json
    settings = Path.home() / ".claude/settings.json"
    try:
        cmd = json.loads(settings.read_text())["statusLine"]["command"]
    except Exception:
        return "no status line set, usage needs one (see bridge/README.md)"
    script = next((Path(w).expanduser() for w in cmd.split() if w.endswith(".sh")), None)
    if not script or not script.exists():
        return "status line is not a .sh script, add the usage line by hand (bridge/README.md)"
    lines = script.read_text().splitlines(keepends=True)
    mine = [i for i, l in enumerate(lines) if "tinyclawd.py usage" in l]
    if remove:
        if not mine:
            return "status line had no usage line"
        backup(script)
        script.write_text("".join(l for i, l in enumerate(lines) if i not in mine))
        return "usage line removed from " + script.name
    if mine:
        return "status line already sends usage"
    at = next((i for i, l in enumerate(lines) if l.strip().startswith("input=$(cat")), None)
    if at is None:
        return "could not find input=$(cat) in %s, add the usage line by hand" % script.name
    backup(script)
    lines.insert(at + 1, USAGE + "\n")
    script.write_text("".join(lines))
    return "usage line added to " + script.name


def main():
    remove = "--remove" in sys.argv
    print(service(remove))
    if remove:
        sys.argv = ["install_hooks", "--remove"]
    else:
        sys.argv = ["install_hooks"]
    install_hooks.main()
    print(status_line(remove))
    if not remove:
        print("done. if claude code is open, restart it so it loads the hooks.")


if __name__ == "__main__":
    main()

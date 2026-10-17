#!/usr/bin/env python3
"""hooks tiny clawd into claude code on linux:
  python3 setup.py                  background service + hooks + usage from the status line
  python3 setup.py --dry-run        only print what it would change
  python3 setup.py --port /dev/X    the service uses this port (default: finds the pet itself)
  python3 setup.py --statusline visible|hidden|none
                                    with no status line yet: add one that shows usage, a hidden
                                    one that only feeds the pet, or none (default: ask)
  python3 setup.py --remove         undo all of it
safe to run again (that is also how you update). every file it changes gets a backup first."""
import argparse
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import install_hooks as ih  # noqa: E402

UNIT_NAME = "tinyclawd-daemon"
UNIT = Path(os.environ.get("XDG_CONFIG_HOME") or Path.home() / ".config") \
    / "systemd/user" / (UNIT_NAME + ".service")
READ_RE = re.compile(r"""^(\s*)(\w+)=["']?\$\(\s*cat(\s+-)?\s*\)["']?\s*(#.*)?$""")


def backup(p):
    shutil.copy2(str(p), str(p) + ".bak-tinyclawd-" + time.strftime("%Y%m%d-%H%M%S"))


# ---------------------------------------------------------------- service

def sd_quote(s):
    # systemd reads % and $ itself, so they are doubled
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"').replace("%", "%%").replace("$", "$$")


def unit_text(port=None):
    cmd = [ih.python(), str(ih.TC), "daemon"] + (["--port", port] if port else [])
    text = (ih.HERE / "tinyclawd-daemon.service").read_text()
    return text.replace("@EXEC@", " ".join(sd_quote(c) for c in cmd))


def systemd_ok():
    if not shutil.which("systemctl"):
        return False
    r = subprocess.run(["systemctl", "--user", "show-environment"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return r.returncode == 0


def service(remove, dry, port=None):
    ctl = ["systemctl", "--user"]
    run_by_hand = "%s %s daemon" % (shlex.quote(ih.python()), shlex.quote(str(ih.TC)))
    if remove:
        if not UNIT.exists():
            return "no service to remove"
        if dry:
            return "would stop and remove the service"
        if shutil.which("systemctl"):
            subprocess.run(ctl + ["disable", "--now", UNIT_NAME], capture_output=True)
        UNIT.unlink()
        if shutil.which("systemctl"):
            subprocess.run(ctl + ["daemon-reload"], capture_output=True)
        return "service stopped and removed"
    if not systemd_ok():
        return "no systemd user session here, start the daemon yourself:\n    " + run_by_hand
    if dry:
        return "would write %s and start it" % UNIT
    UNIT.parent.mkdir(parents=True, exist_ok=True)
    UNIT.write_text(unit_text(port))
    subprocess.run(ctl + ["daemon-reload"], capture_output=True)
    subprocess.run(ctl + ["enable", UNIT_NAME], capture_output=True)
    # restart, not start: after an update the old code would keep running
    r = subprocess.run(ctl + ["restart", UNIT_NAME], capture_output=True, text=True)
    if r.returncode != 0:
        return "service did not start: %s\n    logs: journalctl --user -u %s" % (r.stderr.strip(), UNIT_NAME)
    return "service running (logs: journalctl --user -u %s -f)" % UNIT_NAME


# ---------------------------------------------------------------- status line

KINDS = {"visible": "statusline", "hidden": "usage"}   # our status line commands


def usage_cmd():
    return ih.command("usage")


def our_kind(cmd):
    """'visible' or 'hidden' when the status line command is one we added, else None"""
    if ih.MARK not in cmd:
        return None
    for kind, what in KINDS.items():
        if cmd.rstrip().endswith(" " + what):
            return kind
    return None


def is_ours(cmd):
    return our_kind(cmd) is not None


def later_cmd():
    path = os.path.abspath(__file__)
    home = str(Path.home())
    if path.startswith(home + "/") and " " not in path:
        path = "~" + path[len(home):]
    else:
        path = shlex.quote(path)
    return "python3 %s --statusline visible" % path


QUESTION = """you have no Claude Code status line. tiny clawd reads your usage from it.
  1. add a visible one: shows ctx, 5h and week usage at the bottom of Claude Code, and feeds Clawd
  2. add a hidden one: shows nothing, only feeds Clawd
  3. skip: Clawd won't show usage (add it later with: %s)
"""
ANSWERS = {"": "visible", "1": "visible", "visible": "visible",
           "2": "hidden", "hidden": "hidden",
           "3": "none", "skip": "none", "none": "none"}


def ask_statusline(tin, tout):
    """asks which status line to add. Enter alone = visible, no answer = none"""
    tout.write(QUESTION % later_cmd())
    for _ in range(3):
        tout.write("pick 1, 2 or 3 [1]: ")
        tout.flush()
        a = tin.readline()
        if not a:
            tout.write("\n")
            return "none"
        a = a.strip().lower()
        if a in ANSWERS:
            return ANSWERS[a]
    return "none"


def ask_on_terminal():
    """only when stdin is a terminal, so a script or an agent running setup never hangs.
    install.sh gives us the terminal under curl | bash"""
    try:
        if not sys.stdin.isatty():
            return None
    except (AttributeError, ValueError):
        return None
    try:
        with open("/dev/tty", "r+") as tty:
            return ask_statusline(tty, tty)
    except OSError:
        return ask_statusline(sys.stdin, sys.stderr)


def find_script(cmd):
    """the shell script a status line command runs, or None"""
    try:
        words = shlex.split(os.path.expandvars(cmd))
    except ValueError:
        return None
    for w in words:
        p = Path(w).expanduser()
        if not p.is_file():
            continue
        try:
            with p.open(errors="replace") as f:
                first = f.readline()
        except OSError:
            continue
        if p.suffix == ".sh" or (first.startswith("#!") and re.search(r"\b(ba|z|da)?sh\b", first)):
            return p
    return None


def patch_script(text, remove):
    """adds (or removes) the usage line right after the line that reads stdin.
    returns the new text, or None when there is no input=$(cat) style line to go after"""
    lines = text.splitlines(keepends=True)
    keep = [l for l in lines if "tinyclawd.py usage" not in l]
    if remove:
        return "".join(keep)
    for i, l in enumerate(keep):
        m = READ_RE.match(l.rstrip("\n"))
        if m:
            line = "%sprintf '%%s' \"$%s\" | %s >/dev/null 2>&1 &\n" % (m.group(1), m.group(2), usage_cmd())
            if not l.endswith("\n"):
                keep[i] = l + "\n"
            keep.insert(i + 1, line)
            return "".join(keep)
    return None


def status_line(settings, remove, dry, want=None):
    """usage comes from the status line json. three cases:
    no status line: ask (or take --statusline) and add a visible one, a hidden one, or none
    one of ours: refresh it, switch it to --statusline, or remove it
    yours: add one line to your script, or tell you what to add"""
    sl = settings.get("statusLine")
    cmd = sl.get("command") if isinstance(sl, dict) else None
    def did(now, later):
        return later if dry else now
    what = {"visible": "a status line that shows ctx, 5h and week usage and feeds Clawd",
            "hidden": "a hidden status line that only feeds Clawd (it shows nothing)"}
    if not isinstance(cmd, str) or not cmd.strip():
        if remove:
            return ""
        if want is None and dry:
            return ("you have no claude code status line. pick one with --statusline visible|hidden|none\n"
                    "    (in a terminal setup asks you)")
        if want is None:
            want = ask_on_terminal()
        if want in KINDS:
            settings["statusLine"] = {"type": "command", "command": ih.command(KINDS[want])}
            return did("added ", "would add ") + what[want]
        if want is None:
            return "no claude code status line, so Clawd won't show usage. add one later with:\n    " + later_cmd()
        return "no status line added, Clawd won't show usage. add one later with:\n    " + later_cmd()
    kind = our_kind(cmd)
    if kind:
        if remove or want == "none":
            del settings["statusLine"]
            return did("removed", "would remove") + " our status line"
        kind = want or kind
        new = ih.command(KINDS[kind])
        if new != cmd:
            sl["command"] = new
            if want and want != our_kind(cmd):
                return did("switched", "would switch") + " to " + what[kind]
        return ""
    script = find_script(cmd)
    by_hand = ("add this to your status line script, right after it reads stdin into $input:\n"
               "    printf '%%s' \"$input\" | %s >/dev/null 2>&1 &" % usage_cmd())
    if script is None:
        return "" if remove else "your status line is not a shell script. " + by_hand
    try:
        text = script.read_text()
        new = patch_script(text, remove)
        if new is None:
            return "could not find input=$(cat) in %s. %s" % (script, by_hand)
        if new == text:
            return "" if remove else "status line already sends usage"
        if not dry:
            backup(script)
            script.write_text(new)
    except OSError as e:
        return "could not change %s (%s). %s" % (script, e, by_hand)
    if remove:
        return did("removed", "would remove") + " the usage line from %s" % script
    return did("added", "would add") + " the usage line to %s" % script


# ---------------------------------------------------------------- main

def pyserial_hint():
    py = ih.python()
    r = subprocess.run([py, "-c", "import serial"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if r.returncode == 0:
        return ""
    ids = ""
    try:
        for line in open("/etc/os-release"):
            if line.startswith(("ID=", "ID_LIKE=")):
                ids += " " + line.split("=", 1)[1].strip().strip('"')
    except OSError:
        pass
    for key, cmd in (("arch", "sudo pacman -S python-pyserial"), ("fedora", "sudo dnf install python3-pyserial"),
                     ("suse", "sudo zypper install python3-pyserial"), ("debian", "sudo apt install python3-serial"),
                     ("ubuntu", "sudo apt install python3-serial")):
        if key in ids:
            break
    else:
        cmd = "%s -m pip install --user pyserial" % py
    return "pyserial is missing for %s, the daemon needs it:\n    %s" % (py, cmd)


def main(argv=None):
    p = argparse.ArgumentParser(description="hook tiny clawd into claude code")
    p.add_argument("--remove", action="store_true", help="undo everything")
    p.add_argument("--dry-run", action="store_true", help="print what would change, change nothing")
    p.add_argument("--port", help="serial port for the service, default: find the pet")
    p.add_argument("--statusline", choices=["visible", "hidden", "none"],
                   help="with no status line yet: what to add (default: ask)")
    a = p.parse_args(argv)
    remove, dry = a.remove, a.dry_run

    settings, old = ih.load()
    n = ih.merge_hooks(settings, remove)
    out = [service(remove, dry, a.port)]
    out.append(status_line(settings, remove, dry, a.statusline))
    if dry:
        same = json.loads(old) == settings if old.strip() else not settings
        out.append("%s needs no change" % ih.SETTINGS if same else
                   "would change %d hook events in %s" % (n, ih.SETTINGS))
    else:
        b = ih.save(settings, old)
        out.append("updated %s (%d hook events), backup %s" % (ih.SETTINGS, n, b) if b
                   else "%s already up to date" % ih.SETTINGS)
    out.append(ih.slash_command(remove, dry))
    if not remove:
        out.append(pyserial_hint())
    for line in out:
        if line:
            print(line)
    if dry:
        print("nothing was changed (dry run)")
    elif remove:
        print("done. restart claude code so it drops the hooks. backups (*.bak-tinyclawd-*) were kept")
    else:
        print("done. restart claude code so it loads the hooks")
        print("undo: %s %s --remove" % (shlex.quote(sys.executable), shlex.quote(os.path.abspath(__file__))))


if __name__ == "__main__":
    main()

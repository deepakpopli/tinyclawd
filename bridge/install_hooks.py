#!/usr/bin/env python3
"""adds the tiny clawd hooks to ~/.claude/settings.json.
keeps every hook you already have, saves a backup first, safe to run twice.
  python3 install_hooks.py            add them
  python3 install_hooks.py --remove   take them out again"""
import json
import shutil
import sys
import time
from pathlib import Path

SETTINGS = Path.home() / ".claude/settings.json"
PY = "/usr/bin/python3 %s" % (Path(__file__).resolve().parent / "tinyclawd.py")
CMD = PY + " hook"
SCREEN = PY + " screen"   # runs before a slash command, can stop it (so /clawd costs no claude turn)
COMMAND = Path.home() / ".claude/commands/clawd.md"
COMMAND_TEXT = """---
description: show my claude usage on tiny clawd (try /clawd context)
---
<!-- tinyclawd: the hook shows this on the pet and stops the prompt, claude never sees it -->
show my usage on tiny clawd
"""

# event -> matcher (None = no matcher)
EVENTS = {
    "SessionStart": None, "SessionEnd": None, "UserPromptSubmit": None,
    "PreToolUse": "*", "PostToolUse": "*", "PostToolUseFailure": "*", "PermissionRequest": "*",
    "Notification": None, "Stop": None, "StopFailure": None,
    "SubagentStart": None, "SubagentStop": None, "PreCompact": None,
}


# hooks that must run before claude sees the prompt (not async)
SYNC = {"UserPromptExpansion": SCREEN}


def ours(group):
    return any("tinyclawd.py " in h.get("command", "") for h in group.get("hooks", []))


def slash_command(remove):
    """the /clawd command file, only ever touches it if it's ours"""
    if remove:
        if COMMAND.exists() and "tinyclawd" in COMMAND.read_text():
            COMMAND.unlink()
            return "removed /clawd"
        return ""
    if COMMAND.exists():
        return "" if "tinyclawd" in COMMAND.read_text() else "a /clawd command already exists, left it alone"
    COMMAND.parent.mkdir(parents=True, exist_ok=True)
    COMMAND.write_text(COMMAND_TEXT)
    return "added /clawd"


def main():
    remove = "--remove" in sys.argv
    s = json.loads(SETTINGS.read_text()) if SETTINGS.exists() else {}
    backup = SETTINGS.with_name("settings.json.bak-tinyclawd-%s" % time.strftime("%Y%m%d-%H%M%S"))
    if SETTINGS.exists():
        shutil.copy(SETTINGS, backup)
    hooks = s.setdefault("hooks", {})
    changed = 0
    todo = [(ev, m, CMD, True) for ev, m in EVENTS.items()] + [(ev, None, c, False) for ev, c in SYNC.items()]
    for ev, matcher, cmd, run_async in todo:
        groups = hooks.get(ev, [])
        mine = [g for g in groups if ours(g)]
        if remove:
            if mine:
                hooks[ev] = [g for g in groups if not ours(g)]
                if not hooks[ev]:
                    del hooks[ev]
                changed += 1
            continue
        if mine:
            continue
        h = {"type": "command", "command": cmd, "timeout": 5}
        if run_async:
            h["async"] = True
        g = {"hooks": [h]}
        if matcher:
            g = {"matcher": matcher, **g}
        hooks[ev] = groups + [g]
        changed += 1
    if not hooks:
        del s["hooks"]
    SETTINGS.write_text(json.dumps(s, indent=2) + "\n")
    print("%s %d hooks, backup in %s" % ("removed" if remove else "added", changed, backup.name))
    msg = slash_command(remove)
    if msg:
        print(msg)


if __name__ == "__main__":
    main()

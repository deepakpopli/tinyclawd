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
CMD = "/usr/bin/python3 %s hook" % (Path(__file__).resolve().parent / "tinyclawd.py")

# event -> matcher (None = no matcher)
EVENTS = {
    "SessionStart": None, "SessionEnd": None, "UserPromptSubmit": None,
    "PreToolUse": "*", "PostToolUse": "*", "PostToolUseFailure": "*", "PermissionRequest": "*",
    "Notification": None, "Stop": None, "StopFailure": None,
    "SubagentStart": None, "SubagentStop": None, "PreCompact": None,
}


def ours(group):
    return any("tinyclawd.py hook" in h.get("command", "") for h in group.get("hooks", []))


def main():
    remove = "--remove" in sys.argv
    s = json.loads(SETTINGS.read_text()) if SETTINGS.exists() else {}
    backup = SETTINGS.with_name("settings.json.bak-tinyclawd-%s" % time.strftime("%Y%m%d-%H%M%S"))
    if SETTINGS.exists():
        shutil.copy(SETTINGS, backup)
    hooks = s.setdefault("hooks", {})
    changed = 0
    for ev, matcher in EVENTS.items():
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
        g = {"hooks": [{"type": "command", "command": CMD, "async": True, "timeout": 5}]}
        if matcher:
            g = {"matcher": matcher, **g}
        hooks[ev] = groups + [g]
        changed += 1
    if not hooks:
        del s["hooks"]
    SETTINGS.write_text(json.dumps(s, indent=2) + "\n")
    print("%s %d hooks, backup in %s" % ("removed" if remove else "added", changed, backup.name))


if __name__ == "__main__":
    main()

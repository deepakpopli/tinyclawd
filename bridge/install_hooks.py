#!/usr/bin/env python3
"""adds the tiny clawd hooks to ~/.claude/settings.json.
keeps every hook you already have, backs the file up before it changes it, safe to run again
(a second run only fixes paths, for example after you moved the folder).
  python3 install_hooks.py            add them
  python3 install_hooks.py --remove   take them out again
setup.py does this too, plus the service and the status line."""
import json
import os
import shlex
import shutil
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
TC = HERE / "tinyclawd.py"
MARK = "tinyclawd.py"   # every command we add has this in it, nothing else is touched
CLAUDE = Path(os.environ.get("CLAUDE_CONFIG_DIR") or Path.home() / ".claude")
SETTINGS = CLAUDE / "settings.json"
COMMAND = CLAUDE / "commands" / "clawd.md"
COMMAND_TEXT = """---
description: show my claude usage on tiny clawd (try /clawd context)
---
<!-- tinyclawd: the hook shows this on the pet and stops the prompt, claude never sees it -->
show my usage on tiny clawd
"""

# event -> matcher (None = no matcher). all of these run async, claude never waits for them
EVENTS = {
    "SessionStart": None, "SessionEnd": None, "UserPromptSubmit": None,
    "PreToolUse": "*", "PostToolUse": "*", "PostToolUseFailure": "*", "PermissionRequest": "*",
    "Notification": None, "Stop": None, "StopFailure": None,
    "SubagentStart": None, "SubagentStop": None, "PreCompact": None,
}
# this one runs before claude sees a slash command and can stop it, so /clawd costs no turn
SYNC = {"UserPromptExpansion": "screen"}


def python():
    """the python the hooks and the service run with. /usr/bin/python3 stays put while venvs
    and conda envs come and go, so it wins when it is there"""
    return "/usr/bin/python3" if os.path.exists("/usr/bin/python3") else sys.executable


def command(what):
    return "%s %s %s" % (shlex.quote(python()), shlex.quote(str(TC)), what)


def ours(h):
    return isinstance(h, dict) and MARK in str(h.get("command", ""))


def our_group(ev):
    if ev in SYNC:
        return {"hooks": [{"type": "command", "command": command(SYNC[ev]), "timeout": 5}]}
    h = {"type": "command", "command": command("hook"), "async": True, "timeout": 5}
    return {"matcher": EVENTS[ev], "hooks": [h]} if EVENTS[ev] else {"hooks": [h]}


def merge_hooks(settings, remove=False):
    """puts our hooks into settings (a dict, changed in place), or takes them out.
    other hooks stay where they are. returns how many events changed"""
    hooks = settings.get("hooks")
    if hooks is None:
        hooks = {}
    if not isinstance(hooks, dict):
        raise SystemExit("\"hooks\" in %s is not an object, fix it first" % SETTINGS)
    wanted = {} if remove else {ev: our_group(ev) for ev in list(EVENTS) + list(SYNC)}
    changed = 0
    for ev in list(hooks) + [e for e in wanted if e not in hooks]:
        groups = hooks.get(ev, [])
        if not isinstance(groups, list):
            continue   # not ours to fix
        new, at = [], None
        for g in groups:
            hs = g.get("hooks") if isinstance(g, dict) else None
            if not isinstance(hs, list) or not any(ours(h) for h in hs):
                new.append(g)
                continue
            if at is None:
                at = len(new)   # ours goes back to the same spot
            keep = [h for h in hs if not ours(h)]
            if keep:
                new.append(dict(g, hooks=keep))
        if ev in wanted:
            new.insert(len(new) if at is None else at, wanted[ev])
        if new != groups:
            changed += 1
            if new:
                hooks[ev] = new
            else:
                hooks.pop(ev, None)
    if hooks:
        settings["hooks"] = hooks
    else:
        settings.pop("hooks", None)
    return changed


def load(path=None):
    """settings.json as (dict, text). a missing or empty file is {}"""
    path = Path(path or SETTINGS)
    text = path.read_text() if path.exists() else ""
    if not text.strip():
        return {}, text
    try:
        data = json.loads(text)
    except ValueError as e:
        raise SystemExit("%s is not valid json (%s). fix it first, nothing was changed" % (path, e))
    if not isinstance(data, dict):
        raise SystemExit("%s is not a json object, nothing was changed" % path)
    return data, text


def save(data, old_text, path=None):
    """writes the file only if it changed, after a backup. returns the backup name or ''"""
    path = Path(path or SETTINGS)
    text = json.dumps(data, indent=2) + "\n"
    if old_text.strip() and json.loads(old_text) == data:
        return ""
    real = path.resolve()   # a symlinked settings.json (dotfiles) stays a symlink
    real.parent.mkdir(parents=True, exist_ok=True)
    backup = ""
    if real.exists():
        backup = real.with_name(real.name + ".bak-tinyclawd-" + time.strftime("%Y%m%d-%H%M%S"))
        shutil.copy2(str(real), str(backup))
    tmp = real.with_name(real.name + ".tinyclawd-tmp")
    tmp.write_text(text)
    if real.exists():
        shutil.copymode(str(real), str(tmp))
    os.replace(str(tmp), str(real))
    return backup.name if backup else "(new file)"


def slash_command(remove, dry=False):
    """the /clawd command file, only ever touched if it's ours"""
    mine = COMMAND.exists() and "tinyclawd" in COMMAND.read_text()
    if remove:
        if not mine:
            return ""
        if dry:
            return "would remove /clawd"
        COMMAND.unlink()
        return "removed /clawd"
    if COMMAND.exists():
        return "" if mine else "a /clawd command already exists, left it alone"
    if dry:
        return "would add /clawd"
    COMMAND.parent.mkdir(parents=True, exist_ok=True)
    COMMAND.write_text(COMMAND_TEXT)
    return "added /clawd"


def main():
    remove = "--remove" in sys.argv
    s, old = load()
    n = merge_hooks(s, remove)
    backup = save(s, old)
    if n:
        word = "removed" if remove else "set up"
        print("%s %d hooks in %s, backup %s" % (word, n, SETTINGS, backup))
    else:
        print("no tiny clawd hooks to remove" if remove else "hooks already set up")
    msg = slash_command(remove)
    if msg:
        print(msg)


if __name__ == "__main__":
    main()

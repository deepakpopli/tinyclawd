#!/usr/bin/env python3
"""One page with every animation and game, grouped by when it plays.
  python3 gallery.py        -> out/gallery.html (makes missing gifs first, needs ./sim built)
"when it plays" is read from the pools in brain.cpp, so it stays right."""
import html
import re
import subprocess
from pathlib import Path

HERE = Path(__file__).parent
OUT = HERE / "out"
BRAIN = HERE / "../components/pet/src/brain.cpp"

# pool name -> (group, label)
POOLS = {
    "THINK": ("working", "thinking"), "SUBS": ("working", "subagents running"),
    "T_BASH": ("working", "Bash"), "T_READ": ("working", "Read"), "T_SEARCH": ("working", "Grep / Glob"),
    "T_EDIT": ("working", "Edit / Write"), "T_EDIT_MANY": ("working", "lots of edits"),
    "T_WEB": ("working", "web search / fetch"), "T_TODO": ("working", "TodoWrite"),
    "T_MCP": ("working", "mcp tools"), "T_OTHER": ("working", "other tools"),
    "START": ("events", "session start"), "END": ("events", "session end"),
    "DONE_QUICK": ("events", "done, quick task"), "DONE_MID": ("events", "done, medium task"),
    "DONE_LONG": ("events", "done, long task"), "COMMIT": ("events", "git commit"),
    "TEST_PASS": ("events", "test passed"), "P_YELLOW_CARD": ("events", "test failed"),
    "P_RED_CARD": ("events", "test failed again"), "P_ANGRY": ("events", "third fail in a row"),
    "TOOL_FAIL": ("events", "tool error"), "INTERRUPT": ("events", "esc pressed"),
    "P_SQUISH": ("events", "context compacted"), "P_SUBAGENTS_MARCH": ("events", "subagents start"),
    "P_SUBSTITUTION": ("events", "another subagent joins"), "P_SQUAD_RETURN": ("events", "subagents done"),
    "PERM_IN": ("needs you", "claude asks you"), "WAIT": ("needs you", "waiting for you"),
    "P_TIRED": ("usage", "usage over 80%"), "P_STOPPAGE_TIME": ("usage", "usage over 90%"),
    "P_LIMIT_HIT": ("usage", "limit hit"), "P_FULL_TIME": ("usage", "window reset"),
    "TAP1": ("touch", "tap"), "TAP_MORE": ("touch", "2-3 taps"), "TAP_LOTS": ("touch", "4-5 taps"),
    "TAP_TOO_MANY": ("touch", "6+ taps"), "DOUBLE": ("touch", "double tap"), "LONG": ("touch", "hold"),
    "IDLE": ("idle", "resting face"), "AWAKE": ("idle", "idle, awake"), "BORED": ("idle", "idle, bored"),
    "DROWSY": ("sleep", "getting sleepy"), "SLEEPING": ("sleep", "asleep"), "ASLEEP": ("sleep", "asleep"),
}
GROUPS = ["working", "events", "needs you", "usage", "touch", "idle", "sleep", "music", "other", "games"]


def uses():
    """anim name -> list of labels, from the Pick pools and the little stories"""
    src = BRAIN.read_text()
    out = {}
    for pool, body in re.findall(r"static const Pick (\w+)\[\] = \{(.*?)\};", src, re.S):
        if pool not in POOLS and pool.startswith("MUSIC"):
            POOLS[pool] = ("music", "music playing")
        group, label = POOLS.get(pool, ("other", pool.lower()))
        for name in re.findall(r'\{"([\w:]+)",', body):
            out.setdefault(name, []).append((group, label))
    for seq, steps in re.findall(r'\{"(\w+)", "([^"]+)"\}', src):
        for name in re.split(r"[ |]", steps):
            out.setdefault(name, []).append(("idle", "story: " + seq))
    return out


def main():
    names = [l.split()[0] for l in subprocess.run(["./sim", "list"], cwd=HERE, capture_output=True,
                                                     text=True).stdout.splitlines()]
    games = [l.split()[0] for l in subprocess.run(["./sim", "games"], cwd=HERE, capture_output=True,
                                                     text=True).stdout.splitlines()]
    for n in names:
        if not (OUT / f"{n}.gif").exists():
            subprocess.run(["./sim", "anim", n], cwd=HERE, capture_output=True)
            subprocess.run(["python3", "gif.py", f"out/{n}.frames"], cwd=HERE, capture_output=True)
    u = uses()
    cards = {g: [] for g in GROUPS}
    for n in names:
        where = u.get(n, [("other", "only by name / in other anims")])
        group = min((g for g, _ in where), key=GROUPS.index)
        labels = sorted({l for _, l in where})
        cards[group].append((n, ", ".join(labels)))
    for g in games:
        cards["games"].append((f"game_{g}", "menu > games"))
    parts = []
    for g in GROUPS:
        if not cards[g]:
            continue
        parts.append(f"<h2>{g} <span>{len(cards[g])}</span></h2><div class=grid>")
        for n, label in cards[g]:
            parts.append(f'<figure><img src="{n}.gif" loading=lazy><figcaption><b>{html.escape(n)}</b>'
                         f"<br>{html.escape(label)}</figcaption></figure>")
        parts.append("</div>")
    page = """<!doctype html><meta charset=utf-8><title>tiny clawd animations</title>
<style>
body { background: #16181d; color: #ddd; font: 14px sans-serif; margin: 24px; }
h1 { color: #fff; margin: 0 0 4px; } h2 { color: #e8a07a; margin: 28px 0 10px; text-transform: capitalize; }
h2 span { color: #777; font-size: 14px; } p { color: #999; margin: 0; }
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(260px, 1fr)); gap: 14px; }
figure { margin: 0; background: #20232a; padding: 8px; border-radius: 8px; }
img { width: 100%; image-rendering: pixelated; display: block; border-radius: 4px; }
figcaption { margin-top: 6px; color: #aaa; } b { color: #fff; }
</style><h1>tiny clawd</h1><p>__N__ animations + __G__ games, grouped by when they play</p>
""".replace("__N__", str(len(names))).replace("__G__", str(len(games))) + "\n".join(parts)
    (OUT / "gallery.html").write_text(page)
    print("wrote out/gallery.html")


if __name__ == "__main__":
    main()

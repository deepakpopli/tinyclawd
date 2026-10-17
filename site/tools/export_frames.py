#!/usr/bin/env python3
"""Screen frames for the site, recorded from the simulator.
  python3 site/tools/export_frames.py   -> site/assets/frames/<name>.bin + index.json

Each .bin is deflate (zlib) of:
  uint16 count, count x uint16 ticks (20 ms each), count x 1024 byte frames
A frame is the SH1106 buffer: byte = page * 128 + x, bit = y & 7."""
import json
import struct
import subprocess
import zlib
from pathlib import Path

here = Path(__file__).resolve().parent
sim = here / "../../firmware/sim"
out = here / "../assets/frames"

# the 3D model and the big OLED
STORY = [
    "idle", "wake_up", "thinking_words", "terminal", "searching", "crane", "test_tubes", "git_graph",
    "subagents_march", "subagents", "needs_you", "goal", "bicycle_kick", "yellow_card", "usage",
    "now_playing", "equalizer", "coffee", "juggle", "dream", "asleep", "petted", "love",
]
# the wall of small screens
WALL = [
    "thinking_words", "thinking", "lightbulb", "reading", "typing", "typing_along", "keyboard_peek",
    "letters_fly", "terminal", "searching", "web", "crane", "test_tubes", "git_graph", "git_branch",
    "package", "subagents_march", "subagents", "needs_you", "rubber_duck", "goal", "bicycle_kick",
    "trophy", "bug_squash", "yellow_card", "red_card", "var_check", "context", "usage", "limit_hit",
    "now_playing", "equalizer", "music", "headbang", "dance", "coffee", "juggle", "paper_plane",
    "stargaze", "dream", "petted", "love",
]
ANIMS = list(dict.fromkeys(STORY + WALL))
GAMES = ["run", "panic", "flappy", "keepy", "penalty", "catch", "whack"]
MAX_MS = 17000      # loops are recorded once through, capped here
GAME_MS = 10000


def anim_lengths():
    text = subprocess.run(["./sim", "list"], cwd=sim, capture_output=True, text=True, check=True).stdout
    return {line.split()[0]: int(line.split()[1]) for line in text.splitlines() if line.strip()}


def pack(data):
    frames, ticks = [], []
    for i in range(0, len(data) - 1023, 1024):
        buf = data[i:i + 1024]
        if frames and frames[-1] == buf and ticks[-1] < 65535:
            ticks[-1] += 1
        else:
            frames.append(buf)
            ticks.append(1)
    raw = struct.pack(f"<H{len(ticks)}H", len(ticks), *ticks) + b"".join(frames)
    return zlib.compress(raw, 9), len(frames), sum(ticks) * 20


def main():
    subprocess.run(["make", "-j1"], cwd=sim, check=True, capture_output=True)
    lengths = anim_lengths()
    out.mkdir(parents=True, exist_ok=True)
    (sim / "out").mkdir(exist_ok=True)
    jobs = [(n, ["anim", n, str(min(lengths[n], MAX_MS))]) for n in ANIMS]
    jobs += [("game_" + g, ["game", g, str(GAME_MS)]) for g in GAMES]
    index = {}
    for name, args in jobs:
        subprocess.run(["./sim", *args], cwd=sim, check=True, capture_output=True)
        blob, count, ms = pack((sim / "out" / f"{name}.frames").read_bytes())
        (out / f"{name}.bin").write_bytes(blob)
        index[name] = {"ms": ms, "frames": count, "bytes": len(blob)}
        print(f"{name:18} {ms:6} ms {count:4} frames {len(blob) / 1024:6.1f} KB")
    (out / "index.json").write_text(json.dumps(index, indent=1) + "\n")
    total = sum(v["bytes"] for v in index.values())
    print(f"{len(index)} files, {total / 1024:.0f} KB")


if __name__ == "__main__":
    main()

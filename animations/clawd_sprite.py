"""Mini Clawd sprite, built from the block-character art Claude Code uses
(copied from the Claude Code 2.1.289 CLI). Three text rows of quadrant block
characters; each character becomes 4 x 8 pixels (terminal cells are about
twice as tall as wide), so the sprite is 36 x 24 pixels.
"""
from PIL import ImageDraw

CELL_W, CELL_H = 4, 8
SPRITE_W, SPRITE_H = 9 * CELL_W, 3 * CELL_H

# quadrants: top-left, top-right, bottom-left, bottom-right
QUADS = {
    " ": "0000", "▀": "1100", "▄": "0011", "█": "1111", "▌": "1010", "▐": "0101",
    "▖": "0010", "▗": "0001", "▘": "1000", "▙": "1011", "▚": "1001", "▛": "1110",
    "▜": "1101", "▝": "0100", "▞": "0110", "▟": "0111",
}

# poses, as in Claude Code
ARMS = {
    "down":   {"r1L": " ▐", "r1R": "",  "r2L": "▝▜", "r2R": "█▀"},
    "up":     {"r1L": "▗▟", "r1R": "▄", "r2L": " ▜", "r2R": "█▘"},
    "one-up": {"r1L": " ▐", "r1R": "▄", "r2L": "▝▜", "r2R": "█▘"},
}
EYES = {
    "open": "▛███▛█", "left": "▟███▟█", "right": "█▟███▟",
    "closed": "▂███▂█", "wink": "▛███▂█",
}
FEET = {"both": " ▝▝   ▝▝ ", "left": " ▝▝      ", "right": "      ▝▝ "}

# the 360 turn, rows as in Claude Code
TURN = {
    "right-12": [" ▐█▜██▛█ ", "▝▜██████▀", " ▝▝   ▝▝ "],
    "right-30": ["  █▛██▛▌ ", " ▝█████▛ ", "  ▘▘  ▘▘ "],
    "right-55": ["  ▐█▛█▜  ", "  ▐████  ", "  ▝▝ ▝▝  "],
    "right-75": ["   ██▛▌  ", "   ███▌  ", "   ▘  ▘  "],
    "edge":     ["   ▐██   ", "   ▐██   ", "   ▝ ▝   "],
    "back-105": ["   ███▌  ", "   ███▌  ", "   ▘  ▘  "],
    "back-125": ["  ▐████  ", "  ▐████  ", "  ▝▝ ▝▝  "],
    "back-150": ["  █████▌ ", " ▝█████▛ ", "  ▘▘  ▘▘ "],
    "back":     [" ▐██████ ", "▝▜██████▀", " ▝▝   ▝▝ "],
    "left-75":  ["   ▛██▌  ", "   ███▌  ", "   ▘  ▘  "],
    "left-55":  ["  ▐▜▛██  ", "  ▐████  ", "  ▝▝ ▝▝  "],
    "left-30":  ["  ▛██▛█▌ ", " ▝█████▛ ", "  ▘▘  ▘▘ "],
    "left-12":  [" ▐▛███▜█ ", "▝▜██████▀", " ▝▝   ▝▝ "],
}
TURN_ORDER = ["right-12", "right-30", "right-55", "right-75", "edge", "back-105", "back-125",
              "back-150", "back", "left-75", "left-55", "left-30", "left-12"]


def rows(eyes="open", arms="down", feet="both"):
    a = ARMS[arms]
    r1 = (a["r1L"] + EYES[eyes]).ljust(8) + a["r1R"]
    r2 = a["r2L"] + "█████" + a["r2R"]
    return [r1.ljust(9), r2.ljust(9), FEET[feet]]


def draw_rows(d: ImageDraw.ImageDraw, x, y, text_rows, fill=1, scale=1.0):
    """Draw Claude Code block-art rows at pixel x, y. scale 0.5 = half-size Clawd (18 x 12 px)."""
    cw, ch_ = round(CELL_W * scale), round(CELL_H * scale)
    qw, qh = max(1, cw // 2), max(1, ch_ // 2)
    for r, line in enumerate(text_rows):
        for c, ch in enumerate(line):
            px, py = x + c * cw, y + r * ch_
            if ch == "▂":                       # closed eye: body with a thin lid line
                d.rectangle([px, py, px + cw - 1, py + ch_ - 1], fill=fill)
                d.rectangle([px + qw, py + ch_ - 2, px + cw - 1, py + ch_ - 2], fill=1 - fill)
                continue
            for k, on in enumerate(QUADS.get(ch, "0000")):
                if on == "1":
                    qx, qy = px + (k % 2) * qw, py + (k // 2) * qh
                    d.rectangle([qx, qy, qx + qw - 1, qy + qh - 1], fill=fill)


def draw(d, x, y, eyes="open", arms="down", feet="both", scale=1.0):
    draw_rows(d, x, y, rows(eyes, arms, feet), scale=scale)


def kick_leg(d, x, y, side="right"):
    """A kicking leg, swung forward (Claude Code just hides the lifted foot)."""
    if side == "right":
        d.line([(x + 29, y + 16), (x + 36, y + 13)], fill=1, width=2)
    else:
        d.line([(x + 6, y + 16), (x - 1, y + 13)], fill=1, width=2)

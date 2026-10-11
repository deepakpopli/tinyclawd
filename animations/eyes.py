#!/usr/bin/env python3
"""Clawd eye animations for a 128x64 1-bit OLED.

Renders every animation to gifs/<name>.gif (upscaled with an OLED pixel look)
plus gifs/gallery.html. The drawing is kept to simple shapes (rects, lines,
small bitmaps) so the same logic ports to the ESP32 with U8g2.

  python3 eyes.py            -> render all
  python3 eyes.py blink      -> render one
"""
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import clawd_sprite as cs

W, H = 128, 64
SCALE = 5                      # GIF pixel size
ON = (232, 240, 255)           # lit OLED pixel
OFF = (10, 12, 16)

# default eye: Clawd's tall pixel slits
EYE_W, EYE_H = 12, 22
LEFT_X, RIGHT_X, EYE_Y = 42, 86, 32   # eye centres

OUT = Path(__file__).parent / "gifs"
GLYPH_FONT = ImageFont.truetype("DejaVuSans.ttf", 14)
TEXT_FONT = ImageFont.load_default()

# Claude Code's thinking spinner, as in the CLI: forward then reversed
SPINNER = ["·", "✢", "✳", "✶", "✻", "✽"]
SPINNER = SPINNER + SPINNER[::-1]


# ---------------------------------------------------------------- drawing

class Eye:
    """One eye. shape: rect, happy (^), squint (> or <), x, heart, half,
    sad, angry, spiral. mirror = this is the right eye."""

    def __init__(self, dx=0, dy=0, w=EYE_W, h=EYE_H, shape="rect", lid=0.0, mirror=False,
                 phase=0.0):
        self.dx, self.dy, self.w, self.h = dx, dy, w, h
        self.shape, self.lid, self.mirror = shape, lid, mirror
        self.phase = phase


def thick_line(d, pts, width=3):
    d.line(pts, fill=1, width=width, joint="curve")


def draw_eye(d, cx, cy, e):
    x, y = cx + e.dx, cy + e.dy
    w, h = max(e.w, 1), max(e.h, 2)
    if e.shape == "rect":
        top = y - h / 2 + e.lid * h          # lid comes down from the top
        if y + h / 2 - top >= 2:
            d.rectangle([x - w / 2, top, x + w / 2 - 1, y + h / 2 - 1], fill=1)
        else:
            d.rectangle([x - w / 2, y + h / 2 - 2, x + w / 2 - 1, y + h / 2 - 1], fill=1)
    elif e.shape == "half":                   # tired: flat heavy lid
        d.rectangle([x - w / 2, y, x + w / 2 - 1, y + h / 2 - 1], fill=1)
    elif e.shape == "happy":                  # ^
        thick_line(d, [(x - w / 2 - 2, y + 4), (x, y - 6), (x + w / 2 + 2, y + 4)])
    elif e.shape == "squint":                 # > on the left eye, < on the right
        s = -1 if e.mirror else 1
        thick_line(d, [(x - s * 6, y - 8), (x + s * 6, y), (x - s * 6, y + 8)])
    elif e.shape == "x":
        thick_line(d, [(x - 7, y - 7), (x + 7, y + 7)])
        thick_line(d, [(x - 7, y + 7), (x + 7, y - 7)])
    elif e.shape == "heart":
        heart(d, x, y, e.w / EYE_W)
    elif e.shape in ("sad", "angry"):
        # slanted top edge: sad = inner corner up, angry = inner corner down
        inner_up = (e.shape == "sad")
        top, bot = y - h / 2, y + h / 2 - 1
        hi, lo = top, top + 8
        left_top, right_top = (lo, hi) if inner_up != e.mirror else (hi, lo)
        d.polygon([(x - w / 2, left_top), (x + w / 2 - 1, right_top),
                   (x + w / 2 - 1, bot), (x - w / 2, bot)], fill=1)
    elif e.shape == "spiral":
        pts = []
        for i in range(60):
            t = i / 59 * 3 * math.pi
            r = 1 + 2.3 * t
            a = t + e.phase * (-1 if e.mirror else 1)
            pts.append((x + r * math.cos(a) * 0.9, y + r * math.sin(a) * 0.9))
        d.line(pts, fill=1, width=2)


def heart(d, x, y, k=1.0):
    r = 4 * k
    d.ellipse([x - 2 * r, y - r * 1.5, x, y + r * 0.5], fill=1)
    d.ellipse([x, y - r * 1.5, x + 2 * r, y + r * 0.5], fill=1)
    d.polygon([(x - 2 * r, y - r * 0.4), (x + 2 * r, y - r * 0.4), (x, y + r * 2.2)], fill=1)


def glyph(d, ch, x, y, font=GLYPH_FONT):
    d.text((x, y), ch, fill=1, font=font)


def frame(left=None, right=None, extras=()):
    """Render one 128x64 frame. extras: callables taking the ImageDraw."""
    img = Image.new("1", (W, H), 0)
    d = ImageDraw.Draw(img)
    draw_eye(d, LEFT_X, EYE_Y, left or Eye())
    draw_eye(d, RIGHT_X, EYE_Y, right or Eye(mirror=True))
    for x in extras:
        x(d)
    return img


def both(**kw):
    return Eye(**kw), Eye(mirror=True, **kw)


# smooth motion: loops run in half steps (SUB per original step); numeric lists
# like bounce heights are interpolated between entries, text/pose lists snap
SUB = 2


def steps(n):
    return [k / SUB for k in range((n - 1) * SUB + 1)]


def pick(seq, i):
    i0 = int(math.floor(i)) % len(seq)
    f = i - math.floor(i)
    a, b = seq[i0], seq[(i0 + 1) % len(seq)]
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a + (b - a) * f
    return a


def ms(d):
    return max(20, round(d / SUB))     # GIFs and browsers need 20 ms or more


def ease(t):
    return 0.5 - 0.5 * math.cos(math.pi * t)


def tween(a, b, n):
    """Eased values from a to b (excluding a): n steps, SUB in-betweens each."""
    n = n * SUB
    return [a + (b - a) * ease(i / n) for i in range(1, n + 1)]


# ------------------------------------------------------------ animations
# each returns a list of (frame, ms)

def blink_frames(hold=0, **kw):
    """Blink with a little squash: eyes get wider as they close."""
    out = []
    w = kw.pop("w", EYE_W)
    for h, grow in [(14, 1), (6, 2), (2, 3), (2, 3), (6, 2), (14, 1), (EYE_H, 0)]:
        out.append((frame(*both(h=round(h * kw.get("h", EYE_H) / EYE_H), w=w + grow,
                                **{k: v for k, v in kw.items() if k != "h"})), 35))
    if hold:
        out += breathe(hold, w=w, **kw)
    return out


def breathe(ms, **kw):
    """Hold a pose with a slow 1 px bob so it never looks frozen."""
    dy = kw.pop("dy", 0)
    parts = [(0, 0.35), (1, 0.3), (0, 0.35)]
    return [(frame(*both(dy=dy + b, **kw)), max(20, int(ms * f))) for b, f in parts]


def a_blink():
    """Claude Code: eyes "closed" pose. Idle blink."""
    return breathe(1400) + blink_frames(hold=900) + blink_frames(hold=1200)


def a_look_around():
    """Claude Code: look-left / look-right poses."""
    out = [(frame(), 500)]
    seq, cur = [], 0
    for target, hold in [(-12, 600), (0, 200), (12, 600), (0, 300)]:
        for v in tween(cur, target, 4):
            seq.append((frame(*both(dx=round(v))), ms(40)))
        seq.append((frame(*both(dx=target)), hold))
        cur = target
    return out + seq + blink_frames(hold=600)


def a_wink():
    """Claude Code: wink pose."""
    out = [(frame(), 700)]
    for h in [14, 6, 2]:
        out.append((frame(Eye(), Eye(h=h, mirror=True)), 40))
    out.append((frame(Eye(), Eye(h=2, mirror=True), extras=[lambda d: glyph(d, "✦", 104, 4)]), 500))
    for h in [6, 14, EYE_H]:
        out.append((frame(Eye(), Eye(h=h, mirror=True)), 40))
    return out + [(frame(), 700)]


def a_turn_around():
    """Claude Code: the 360 turn (right-12 .. edge .. back .. left-12)."""
    # (shift, spacing factor, eye width factor, left visible, right visible)
    steps = [(0, 1, 1, 1, 1), (5, .9, .9, 1, 1), (12, .75, .8, 1, 1), (20, .5, .6, 1, 1),
             (28, .25, .4, 0, 1), (34, 0, .25, 0, 1), (0, 0, 0, 0, 0), (0, 0, 0, 0, 0),
             (-34, 0, .25, 1, 0), (-28, .25, .4, 1, 0), (-20, .5, .6, 1, 1), (-12, .75, .8, 1, 1),
             (-5, .9, .9, 1, 1), (0, 1, 1, 1, 1)]
    out = [(frame(), 600)]
    half = (RIGHT_X - LEFT_X) / 2
    for shift, spacing, wf, lv, rv in steps:
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        c = W / 2 + shift
        if lv:
            draw_eye(d, c - half * spacing, EYE_Y, Eye(w=max(2, EYE_W * wf)))
        if rv:
            draw_eye(d, c + half * spacing, EYE_Y, Eye(w=max(2, EYE_W * wf)))
        out.append((img, 90))
    return out + [(frame(), 600)]


def a_thinking():
    """Claude Code thinking spinner, eyes looking up while it works."""
    out = []
    for i in steps(len(SPINNER) * 3):
        drift = round(3 * math.sin(i / 6))
        out.append((frame(*both(dx=-5 + drift, dy=-6),
                          extras=[lambda d, ch=pick(SPINNER, i): glyph(d, ch, 108, 2)]), ms(110)))
    return out


def a_working():
    """New: reading and typing while a tool runs."""
    out = []
    for i in steps(36):
        dx = pick([-6, -3, 0, 3, 6, 6, 3, 0, -3, -6, -6, -6], i)

        def dots(d, i=i):
            for k in range(3):
                on = (i // 3) % 4 > k
                if on:
                    d.rectangle([56 + k * 7, 56, 59 + k * 7, 59], fill=1)
        out.append((frame(*both(dx=dx, dy=6, h=18), extras=[dots]), ms(80)))
    return out


def a_needs_you():
    """New: Claude needs permission. Wide eyes, bounce, blinking '!'."""
    out = []
    for i in steps(16):
        dy = pick([0, -3, -4, -3, 0, 0, 0, 0], i)
        bang = (i // 4) % 2 == 0
        ex = [lambda d: (d.rectangle([62, 2, 65, 11], fill=1), d.rectangle([62, 14, 65, 17], fill=1))] if bang else []
        out.append((frame(*both(dy=dy + 3, w=15, h=26), extras=ex), ms(90)))
    return out


def a_done():
    """New: task finished. Happy ^ ^ eyes, bounce and sparkles."""
    out = [(frame(), 300)]
    for h in [14, 6]:
        out.append((frame(*both(h=h)), 40))
    for i in steps(14):
        dy = pick([0, -4, -6, -4, 0, 0, 0], i)
        sp = (i // 2) % 2 == 0

        def sparkles(d, on=sp):
            for (x, y) in ([(14, 10), (112, 14), (20, 50), (108, 48)] if on else [(10, 20), (116, 34), (26, 56), (102, 56)]):
                d.line([(x - 3, y), (x + 3, y)], fill=1)
                d.line([(x, y - 3), (x, y + 3)], fill=1)
        out.append((frame(*both(shape="happy", dy=dy), extras=[sparkles]), ms(90)))
    for h in [6, 14, EYE_H]:
        out.append((frame(*both(h=h)), 40))
    return out + [(frame(), 500)]


def zzz(d, t):
    for k, (size, x0, y0) in enumerate([(11, 98, 34), (14, 104, 18), (17, 110, 0)]):
        p = (t + k * 0.33) % 1.0
        x, y = x0 + 4 * p, y0 - 6 * p
        f = ImageFont.truetype("DejaVuSans-Bold.ttf", size)
        if p < 0.85:
            d.text((x, y), "z", fill=1, font=f)


def a_sleepy():
    """New: idle for a while. Lids droop, slow blink, falls asleep, Zz."""
    out = [(frame(), 600)]
    for lid in tween(0, 0.45, 8):
        out.append((frame(*both(lid=lid)), ms(70)))
    out.append((frame(*both(lid=0.45)), 600))
    for lid in tween(0.45, 0.9, 6) + tween(0.9, 0.5, 6):
        out.append((frame(*both(lid=lid)), ms(60)))
    out.append((frame(*both(lid=0.5)), 500))
    for lid in tween(0.5, 1.0, 8):
        out.append((frame(*both(lid=lid)), ms(80)))
    for i in steps(30):
        out.append((frame(*both(lid=1.0), extras=[lambda d, t=i / 30: zzz(d, t * 2)]), ms(90)))
    return out


def a_wake_up():
    """New: you came back or Claude pinged. Opens, flutters, then wide awake."""
    out = [(frame(*both(lid=1.0)), 700)]
    for lid in tween(1.0, 0.6, 5):
        out.append((frame(*both(lid=lid)), ms(60)))
    out.append((frame(*both(lid=0.6)), 300))
    for lid in tween(0.6, 1.0, 3) + tween(1.0, 0.0, 6):
        out.append((frame(*both(lid=lid)), ms(45)))
    out += blink_frames(hold=120)
    for s in tween(0, 1, 4):
        out.append((frame(*both(w=round(EYE_W + 4 * s), h=round(EYE_H + 5 * s))), ms(50)))
    out.append((frame(*both(w=16, h=27)), 400))
    for s in tween(1, 0, 4):
        out.append((frame(*both(w=round(EYE_W + 4 * s), h=round(EYE_H + 5 * s))), ms(50)))
    return out + [(frame(), 600)]


def a_petted():
    """New: touch pad on the head. Squints > < and a heart floats up."""
    out = [(frame(), 300)]
    for i in steps(18):
        p = i / 17
        out.append((frame(Eye(shape="squint", dy=2), Eye(shape="squint", dy=2, mirror=True),
                          extras=[lambda d, p=p: heart(d, 64, 46 - 34 * p, 0.9 + 0.3 * p)]), ms(70)))
    return out + blink_frames(hold=600)


def sweat(d, t):
    y = 14 + 20 * t
    d.polygon([(103, y), (100, y + 6), (106, y + 6)], fill=1)
    d.ellipse([100, y + 3, 106, y + 9], fill=1)


def a_tired():
    """New: usage limit above 80%. Heavy lids, slow blink, sweat drop."""
    out = []
    for i in steps(20):
        out.append((frame(*both(shape="half", dy=2), extras=[lambda d, t=i / 19: sweat(d, t)]), ms(90)))
    for lid in tween(0.5, 1.0, 5) + tween(1.0, 0.5, 5):
        out.append((frame(*both(lid=lid, dy=2)), ms(80)))
    return out + [(frame(*both(shape="half", dy=2)), 500)]


def a_limit_hit():
    """New: usage limit reached. X X eyes, wobble."""
    out = []
    for i in steps(20):
        dx = round(3 * math.sin(i / 2))
        out.append((frame(*both(shape="x", dx=dx)), ms(80)))
    return out


def a_surprised():
    """New: something unexpected (error, interrupt). Pop, then settle."""
    out = [(frame(), 500)]
    for s in [1.4, 1.6, 1.5]:
        out.append((frame(*both(w=round(EYE_W * s), h=round(EYE_H * 1.25), dy=-2)), 60))
    out.append((frame(*both(w=round(EYE_W * 1.5), h=round(EYE_H * 1.25), dy=-2),
                      extras=[lambda d: glyph(d, "!", 61, 0, ImageFont.truetype("DejaVuSans-Bold.ttf", 14))]), 700))
    for s in tween(1.5, 1.0, 5):
        out.append((frame(*both(w=round(EYE_W * s), h=round(EYE_H * (1 + (s - 1) / 2)))), ms(50)))
    return out + [(frame(), 500)]


def a_love():
    """New: you hit a streak / ship something big. Pulsing heart eyes."""
    out = []
    for i in steps(16):
        k = 1.0 + 0.25 * (1 + math.sin(i / 16 * 4 * math.pi)) / 2
        out.append((frame(*both(shape="heart", w=EYE_W * k)), ms(80)))
    return out


def a_usage():
    """Status screen: small eyes on the left, 5 hour and weekly limits."""
    out = []
    for i in steps(24):
        fill5 = min(0.42, i / 12 * 0.42)
        fillw = min(0.18, i / 12 * 0.18)

        def bars(d, f5=fill5, fw=fillw):
            for row, (label, f) in enumerate([("5h", f5), ("wk", fw)]):
                y = row * 22
                d.text((44, y), f"{label}  {int(f * 100)}%", fill=1, font=TEXT_FONT)
                d.rectangle([44, y + 12, 123, y + 18], outline=1)
                d.rectangle([46, y + 14, 46 + int(75 * f), y + 16], fill=1)
            d.text((44, 50), "resets 14:30", fill=1, font=TEXT_FONT)
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        h = 2 if i in (16, 17) else 14
        draw_eye(d, 12, 30, Eye(w=7, h=h))
        draw_eye(d, 26, 30, Eye(w=7, h=h))
        bars(d)
        out.append((img, ms(90)))
    return out


# ----------------------------------------------------- mini Clawd (sprite)

def sprite_frame(x, y, extras=(), **pose):
    img = Image.new("1", (W, H), 0)
    d = ImageDraw.Draw(img)
    cs.draw(d, round(x), round(y), **pose)
    for e in extras:
        e(d)
    return img


def a_spin():
    """Claude Code: mini Clawd doing the real 360 turn from the CLI."""
    x, y = (W - cs.SPRITE_W) / 2, 20
    out = [(sprite_frame(x, y), 600)]
    for k in cs.TURN_ORDER:
        img = Image.new("1", (W, H), 0)
        cs.draw_rows(ImageDraw.Draw(img), round(x), y, cs.TURN[k])
        out.append((img, 85))
    out.append((sprite_frame(x, y), 200))
    out.append((sprite_frame(x, y, eyes="wink"), 500))
    return out + [(sprite_frame(x, y), 400)]


def a_wave():
    """Claude Code poses: mini Clawd waves hello (one-up / up arms, wink)."""
    x, y = (W - cs.SPRITE_W) / 2, 20
    out = [(sprite_frame(x, y), 400)]
    for arms in ["one-up", "down", "one-up", "down", "one-up"]:
        out.append((sprite_frame(x, y, arms=arms), 160))
    out.append((sprite_frame(x, y - 3, arms="up"), 120))
    out.append((sprite_frame(x, y - 5, arms="up", eyes="wink"), 500))
    out.append((sprite_frame(x, y - 2, arms="up"), 100))
    return out + [(sprite_frame(x, y), 500)]


# ------------------------------------------------------------- football

def ball(d, x, y, r=3):
    d.ellipse([x - r, y - r, x + r, y + r], fill=1)
    if r >= 4:                                  # panel pattern on a big ball
        d.rectangle([x - 1, y - 1, x + 1, y + 1], fill=0)
        for a in range(0, 360, 72):
            px, py = x + 0.7 * r * math.cos(math.radians(a - 90)), y + 0.7 * r * math.sin(math.radians(a - 90))
            d.point((px, py), fill=0)
    else:
        d.point((x, y), fill=0)


def stars(d, x, y, t):
    for k in range(3):
        a = t * 2 * math.pi + k * 2 * math.pi / 3
        px, py = x + 12 * math.cos(a), y + 4 * math.sin(a)
        d.line([(px - 2, py), (px + 2, py)], fill=1)
        d.line([(px, py - 2), (px, py + 2)], fill=1)


def a_ball_watch():
    """Football: a ball bounces past, the eyes follow it, then it bonks an eye."""
    out = []
    n = 34
    for i in steps(n):                          # left to right, three bounces
        t = i / (n - 1)
        bx = -8 + 144 * t
        by = 56 - 40 * abs(math.sin(t * 3 * math.pi))
        dx = max(-12, min(12, (bx - 64) / 5))
        dy = max(-6, min(6, (by - 32) / 4))
        out.append((frame(*both(dx=round(dx), dy=round(dy)),
                          extras=[lambda d, bx=bx, by=by: ball(d, bx, by, 5)]), ms(45)))
    out += breathe(300, dx=12, dy=4)
    for i in steps(12):                         # comes back and hits the right eye
        t = i / 11
        bx = 136 - 50 * t
        by = 40 - 30 * math.sin(t * math.pi) + 2 * t
        dx = max(-12, min(12, (bx - 64) / 5))
        out.append((frame(*both(dx=round(dx), dy=-2),
                          extras=[lambda d, bx=bx, by=by: ball(d, bx, by, 5)]), ms(45)))
    for i in steps(16):                         # bonk: > < and stars, ball drops away
        t = i / 15
        out.append((frame(Eye(shape="squint", dx=-2), Eye(shape="squint", mirror=True, dx=2),
                          extras=[lambda d, t=t: stars(d, 86, 10, t * 2),
                                  lambda d, t=t: ball(d, 92 + 30 * t, 22 + 40 * t * t, 5)]), ms(60)))
    return out + blink_frames(hold=500)


def a_keepy_uppy():
    """Football: mini Clawd juggles the ball, big kick, it lands on its head."""
    x, y = 40, 38
    out = []
    feet_x = {"right": x + 34, "left": x + 2}
    for hop, side in enumerate(["right", "left", "right", "left"]):
        bx = feet_x[side]
        nb = feet_x["left" if side == "right" else "right"]
        for i in steps(12):
            t = i / 11
            hx = bx + (nb - bx) * t
            hy = 52 - 34 * math.sin(t * math.pi)
            kick = i < 2

            def ex(d, hx=hx, hy=hy, kick=kick, side=side):
                if kick:
                    cs.kick_leg(d, x, y, side)
                ball(d, hx, hy, 3)
            feet = ("left" if side == "right" else "right") if kick else "both"
            out.append((sprite_frame(x, y, extras=[ex], feet=feet), ms(55)))
    for i in steps(10):                         # big kick off the top
        t = i / 9
        out.append((sprite_frame(x, y, feet="left" if i < 2 else "both",
                                 arms="up" if i > 3 else "down",
                                 extras=[lambda d, t=t, i=i: (cs.kick_leg(d, x, y) if i < 2 else None,
                                                              ball(d, x + 34 - 14 * t, 52 - 70 * t, 3))]), ms(55)))
    out.append((sprite_frame(x, y, arms="up"), 400))
    for i in steps(8):                          # falls back on its head
        t = i / 7
        out.append((sprite_frame(x, y, arms="up", eyes="open",
                                 extras=[lambda d, t=t: ball(d, x + 18, -6 + 40 * t * t, 3)]), ms(50)))
    for i in steps(14):
        t = i / 13
        out.append((sprite_frame(x, y + (2 if i < 3 else 0), eyes="closed",
                                 extras=[lambda d, t=t: stars(d, x + 18, y - 6, t * 2),
                                         lambda d, t=t: ball(d, x + 18 + 50 * t, 34 - 20 * math.sin(t * math.pi) + 26 * t, 3)]), ms(60)))
    return out + [(sprite_frame(x, y), 500)]


def goal_frame(d, bulge=0):
    d.line([(104, 30), (127, 30)], fill=1)       # crossbar
    d.line([(104, 30), (104, 62)], fill=1)       # post
    for gy in range(33, 62, 4):                  # net
        for gx in range(107, 127, 4):
            d.point((gx + (bulge if 38 < gy < 56 else 0), gy), fill=1)
    d.line([(0, 63), (127, 63)], fill=1)         # pitch line


def a_goal():
    """Football: run-up, kick, GOAL!, arms up and the 360 turn."""
    y = 38
    out = []
    bx, by = 54, 59
    for i in steps(8):                           # run-up, feet alternate
        x = 4 + 2.5 * i
        out.append((sprite_frame(x, y, feet=pick(["left", "right"], i),
                                 extras=[goal_frame, lambda d: ball(d, bx, by, 3)]), ms(90)))
    x = 4 + 2.5 * 8
    out.append((sprite_frame(x, y, feet="left", extras=[goal_frame, lambda d: ball(d, bx, by, 3)]), 150))
    for i in steps(10):                          # kick and flight into the net
        t = i / 9
        k = i < 2
        out.append((sprite_frame(x, y, feet="left" if k else "both",
                                 extras=[goal_frame, lambda d, k=k: cs.kick_leg(d, x, y) if k else None,
                                         lambda d, t=t: ball(d, bx + 66 * t, by - 26 * math.sin(t * math.pi * 0.8) - 6 * t, 3)]), ms(50)))
    font = ImageFont.truetype("DejaVuSans-Bold.ttf", 16)
    for i in steps(14):                          # GOAL!
        jump = pick([0, -4, -6, -4, 0, 0], i)

        def ex(d, i=i):
            goal_frame(d, bulge=2 if i < 4 else 0)
            ball(d, 120, 52, 3)
            if i % 4 < 3:
                d.text((40, 2), "GOAL!", fill=1, font=font)
        out.append((sprite_frame(x, y + jump, arms="up", extras=[ex]), ms(80)))
    for k in cs.TURN_ORDER:                      # celebration turn
        img = Image.new("1", (W, H), 0)
        dd = ImageDraw.Draw(img)
        goal_frame(dd)
        ball(dd, 120, 52, 3)
        cs.draw_rows(dd, round(x), y, cs.TURN[k])
        out.append((img, 70))
    return out + [(sprite_frame(x, y, eyes="wink", extras=[goal_frame, lambda d: ball(d, 120, 52, 3)]), 700)]


# ------------------------------------------------------------ > < squints

def squints(dx=0, dy=0):
    return Eye(shape="squint", dx=dx, dy=dy), Eye(shape="squint", dx=-dx, dy=dy, mirror=True)


def a_giggle():
    """> <: giggling, a quick shake and bounce."""
    out = [(frame(), 300)]
    for i in steps(22):
        sh = pick([-2, 2], i)
        b = pick([0, -2, -3, -2], i)

        def ticks(d, on=i % 4 < 2):
            if on:
                for (x0, y0, x1, y1) in [(20, 22, 26, 20), (20, 32, 27, 32), (20, 42, 26, 44),
                                         (108, 20, 102, 22), (108, 32, 101, 32), (108, 44, 102, 42)]:
                    d.line([(x0, y0), (x1, y1)], fill=1)
        out.append((frame(*squints(dx=sh, dy=b + 2), extras=[ticks]), ms(60)))
    return out + blink_frames(hold=500)


def blush(d, x, y):
    for k in range(3):
        d.line([(x + k * 4, y + 3), (x + k * 4 + 3, y)], fill=1)


def a_shy():
    """> <: shy, looks down, blush lines under the eyes."""
    out = [(frame(), 300)]
    for i in steps(28):
        sway = round(2 * math.sin(i / 27 * 2 * math.pi))
        out.append((frame(*squints(dx=sway, dy=4),
                          extras=[lambda d, s=sway: (blush(d, LEFT_X - 8 + s, 48), blush(d, RIGHT_X - 4 + s, 48))]), ms(80)))
    return out + blink_frames(hold=500)


def tear(d, x, y):
    d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=1)
    d.point((x, y - 3), fill=1)


def a_laugh():
    """> <: laughing so hard tears fly out."""
    out = []
    for i in steps(24):
        b = pick([0, -4, -5, -4, 0, 1], i)
        t = (i % 12) / 11

        def tears(d, t=t, b=b):
            tear(d, LEFT_X - 12 - 18 * t, 30 + b - 14 * math.sin(t * math.pi) + 12 * t)
            tear(d, RIGHT_X + 12 + 18 * t, 30 + b - 14 * math.sin(t * math.pi) + 12 * t)
        out.append((frame(*squints(dy=b), extras=[tears]), ms(60)))
    return out + blink_frames(hold=500)


# -------------------------------------------------------- more moods/states

def a_sad():
    """Mood: sad (task failed). Droopy eyes and a tear."""
    out = []
    for i in steps(26):
        t = i / 25
        out.append((frame(Eye(shape="sad", dy=4), Eye(shape="sad", dy=4, mirror=True),
                          extras=[lambda d, t=t: tear(d, LEFT_X + 3, 46 + 16 * t)]), ms(80)))
    return out + [(frame(Eye(shape="sad", dy=4), Eye(shape="sad", dy=4, mirror=True)), 600)]


def a_angry():
    """Mood: angry (tests failing again). Slanted brows and a shake."""
    out = []
    for i in steps(20):
        sh = pick([-1, 1], i) if i % 8 < 4 else 0
        out.append((frame(Eye(shape="angry", dx=sh), Eye(shape="angry", dx=sh, mirror=True),
                          extras=[lambda d: (d.line([(110, 4), (114, 8)], fill=1), d.line([(118, 4), (114, 8)], fill=1),
                                             d.line([(110, 14), (114, 10)], fill=1), d.line([(118, 14), (114, 10)], fill=1))]), ms(70)))
    return out


def a_dizzy():
    """Mood: dizzy (too many retries). Spinning spiral eyes."""
    out = []
    for i in steps(24):
        ph = i / 24 * 2 * math.pi
        wob = round(3 * math.sin(i / 4))
        out.append((frame(Eye(shape="spiral", phase=ph, dx=wob), Eye(shape="spiral", phase=ph, dx=wob, mirror=True)), ms(60)))
    return out


def a_searching():
    """State: searching the code (Grep / Glob). Small eyes sweep, then found it."""
    out = []
    for p in range(2):
        for i in steps(14):
            t = i / 13
            dx = round(-22 + 44 * (t if p == 0 else 1 - t))
            out.append((frame(*both(dx=dx, w=8, h=8),
                              extras=[lambda d, dx=dx: d.line([(64 + dx * 2 - 10, 56), (64 + dx * 2 + 10, 56)], fill=1)]), ms(45)))
    for s in tween(0, 1, 4):
        out.append((frame(*both(w=round(8 + 6 * s), h=round(8 + 18 * s))), ms(45)))
    out.append((frame(*both(w=14, h=26), extras=[lambda d: glyph(d, "!", 61, 0, ImageFont.truetype("DejaVuSans-Bold.ttf", 14))]), 600))
    return out + blink_frames(hold=400)


def a_party():
    """State: committed / PR merged. ^ ^ with falling confetti."""
    import random
    rnd = random.Random(7)
    bits = [(rnd.uniform(0, W), rnd.uniform(-60, 0), rnd.uniform(0.6, 1.4), rnd.choice([0, 1])) for _ in range(26)]
    out = []
    for i in steps(30):
        b = pick([0, -3, -4, -3, 0, 0], i)

        def confetti(d, i=i):
            for (x0, y0, v, kind) in bits:
                x = x0 + 4 * math.sin(i / 3 + x0)
                y = (y0 + i * 3 * v) % 80 - 10
                if kind:
                    d.rectangle([x, y, x + 1, y + 1], fill=1)
                else:
                    d.line([(x, y), (x + 1, y + 2)], fill=1)
        out.append((frame(*both(shape="happy", dy=b), extras=[confetti]), ms(70)))
    return out


def a_music():
    """State: session open, nothing running. Vibing with notes."""
    font = ImageFont.truetype("DejaVuSans.ttf", 12)
    out = []
    for i in steps(32):
        sway = round(5 * math.sin(i / 32 * 4 * math.pi))
        bob = round(1.5 * abs(math.sin(i / 32 * 8 * math.pi)))

        def notes(d, i=i):
            for k, ch in enumerate("♪♫"):
                p = ((i / 32) * 2 + k * 0.5) % 1
                d.text((100 + 8 * math.sin(p * 6 + k), 46 - 46 * p), ch, fill=1, font=font)
        out.append((frame(Eye(shape="happy", dx=sway, dy=bob + 2), Eye(shape="happy", dx=sway, dy=bob + 2, mirror=True),
                          extras=[notes]), ms(70)))
    return out


def a_peek():
    """State: idle, pops up from the bottom, looks around, ducks back."""
    out = [(frame(*both(dy=48)), 500)]
    for v in tween(48, 18, 6):
        out.append((frame(*both(dy=round(v))), ms(50)))
    for dx, hold in [(-10, 400), (10, 400), (0, 200)]:
        out.append((frame(*both(dy=18, dx=dx)), hold))
    for v in tween(18, 0, 4):
        out.append((frame(*both(dy=round(v))), ms(45)))
    out += blink_frames(hold=600)
    for v in tween(0, 48, 5):
        out.append((frame(*both(dy=round(v))), ms(45)))
    return out + [(frame(*both(dy=48)), 500)]


# ------------------------------------------------- more football + match mode

BOLD = "DejaVuSans-Bold.ttf"


def sprite_img(**pose):
    """The mini Clawd on its own transparent 1-bit image (for flips and spins)."""
    img = Image.new("1", (cs.SPRITE_W + 8, cs.SPRITE_H), 0)
    cs.draw(ImageDraw.Draw(img), 0, 0, **pose)
    return img


def paste_sprite(img, spr, x, y):
    img.paste(1, (round(x), round(y)), spr)


def goal_centre(d):
    d.line([(30, 16), (98, 16)], fill=1)
    d.line([(30, 16), (30, 62)], fill=1)
    d.line([(98, 16), (98, 62)], fill=1)
    for gy in range(20, 62, 5):
        for gx in range(34, 96, 5):
            d.point((gx, gy), fill=1)
    d.line([(0, 63), (127, 63)], fill=1)


def a_penalty_save():
    """Football: Clawd in goal dives and saves a penalty."""
    out = []
    for i in steps(6):                          # keeper ready, bouncing on its feet
        f = sprite_frame(46, 38 - (i % 2), extras=[goal_centre, lambda d: ball(d, 64, 58, 6)], feet=pick(["left", "right"], i))
        out.append((f, ms(120)))
    for i in steps(10):                          # shot to the top left, keeper dives
        t = i / 9
        bx, by, br = 64 - 26 * t, 58 - 32 * t, 6 - 3 * t
        kx, ky = 46 - 22 * ease(t), 38 - 10 * math.sin(t * math.pi * 0.9)
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        goal_centre(d)
        spr = sprite_img(arms="up" if t > 0.3 else "down")
        if t > 0.4:
            spr = spr.rotate(25 * min(1, (t - 0.4) * 3), expand=True)
        paste_sprite(img, spr, kx, ky)
        ball(d, bx, by, br)
        out.append((img, ms(55)))
    for i in steps(12):                          # caught, lands, SAVE!
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        goal_centre(d)
        y = 30 + min(8, i * 2)
        paste_sprite(img, sprite_img(arms="up", eyes="wink" if i > 6 else "open"), 24, y)
        ball(d, 40, y - 3, 3)
        if i % 4 < 3:
            d.text((52, 0), "SAVE!", fill=1, font=ImageFont.truetype(BOLD, 14))
        out.append((img, ms(90)))
    return out


def a_header():
    """Football: a cross comes in, Clawd jumps and heads it in."""
    out = []
    x0 = 44
    for i in steps(16):
        t = i / 15
        bx, by = -6 + 62 * t, 6 + 22 * t + 10 * math.sin(t * math.pi)
        jump = 10 * math.sin(max(0, (t - 0.5) * 2) * math.pi / 2) if t > 0.5 else 0
        out.append((sprite_frame(x0, 38 - jump, eyes="left" if t < 0.6 else "open",
                                 extras=[goal_frame, lambda d, bx=bx, by=by: ball(d, bx, by, 3)]), ms(55)))
    for i in steps(10):                          # heads it into the net
        t = i / 9
        bx, by = 58 + 60 * t, 24 + 22 * t
        out.append((sprite_frame(x0, 28 + 10 * t, eyes="closed" if i < 3 else "open",
                                 extras=[goal_frame, lambda d, bx=bx, by=by: ball(d, bx, by, 3)]), ms(55)))
    for i in steps(10):
        img = sprite_frame(x0, 38 - pick([0, 3, 5, 3, 0], i), arms="up",
                           extras=[lambda d, i=i: (goal_frame(d, bulge=2 if i < 3 else 0), ball(d, 120, 52, 3),
                                                   d.text((40, 0), "GOAL!", fill=1, font=ImageFont.truetype(BOLD, 16)) if i % 4 < 3 else None)])
        out.append((img, ms(90)))
    return out


def a_bicycle_kick():
    """Football: the ball drops, Clawd flips upside down and volleys it in."""
    out = []
    x0, y0 = 40, 38
    for i in steps(8):                          # ball dropping from the sky
        t = i / 7
        out.append((sprite_frame(x0, y0, eyes="open", extras=[goal_frame, lambda d, t=t: ball(d, 58, -4 + 24 * t, 3)]), ms(60)))
    for k, ang in enumerate([0, 60, 120, 180, 180, 180]):   # jump and flip
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        goal_frame(d)
        spr = sprite_img(eyes="closed" if ang == 180 else "open", feet="left" if ang == 180 else "both")
        spr = spr.rotate(ang, expand=True)
        paste_sprite(img, spr, x0 - 4, y0 - 8 - 4 * min(k, 3))
        ball(d, 58, 20 + 2 * k if k < 4 else 26, 3)
        out.append((img, 70))
    for i in steps(10):                          # ball flies into the net, Clawd lands
        t = i / 9
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        goal_frame(d)
        ang = 180 * (1 - t)
        paste_sprite(img, sprite_img().rotate(ang, expand=True), x0 - 4 * (1 - t), y0 - 20 * (1 - t))
        ball(d, 58 + 62 * t, 26 + 26 * t, 3)
        out.append((img, ms(55)))
    for i in steps(10):
        out.append((sprite_frame(x0, y0 - pick([0, 3, 5, 3, 0], i), arms="up",
                                 extras=[lambda d, i=i: (goal_frame(d, bulge=2 if i < 3 else 0), ball(d, 120, 52, 3),
                                                         d.text((40, 0), "GOAL!", fill=1, font=ImageFont.truetype(BOLD, 16)) if i % 4 < 3 else None)]), ms(90)))
    return out


def a_dribble():
    """Football: dribbles across the screen, weaving through cones."""
    out = []
    cones = [30, 64, 98]

    def ground(d):
        d.line([(0, 63), (127, 63)], fill=1)
        for c in cones:
            d.polygon([(c - 3, 62), (c + 3, 62), (c, 54)], fill=1)
    for i in steps(40):
        t = i / 39
        x = -36 + 168 * t
        weave = 3 * math.sin(t * 6 * math.pi)
        bx = x + 38 + 2 * math.sin(i)
        out.append((sprite_frame(x, 37 + weave * 0.3, feet=pick(["left", "right"], i),
                                 extras=[ground, lambda d, bx=bx: ball(d, bx, 59, 3)]), ms(60)))
    return out


def a_knee_slide():
    """Football: goal celebration, knee slide across the pitch with arms up."""
    out = []
    for i in steps(22):
        t = i / 21
        x = -30 + 110 * ease(t)
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        d.line([(0, 63), (127, 63)], fill=1)
        cs.draw_rows(d, round(x), 47, cs.rows(arms="up", eyes="open")[:2])   # no feet: on its knees
        for k in range(5):                       # grass flying behind
            if t < 0.9:
                d.point((x - 4 - k * 5 - (i * 3 + k * 7) % 4, 60 - (k * 3 + i) % 6), fill=1)
        if t > 0.6 and i % 4 < 3:
            d.text((26, 6), "GOAL!", fill=1, font=ImageFont.truetype(BOLD, 18))
        out.append((img, ms(60)))
    return out + [(out[-1][0], 600)]


def trophy(d, x, y):
    d.pieslice([x - 8, y - 6, x + 8, y + 10], 0, 180, fill=1)       # cup
    d.rectangle([x - 8, y - 2, x + 8, y + 2], fill=1)
    d.arc([x - 13, y - 2, x - 5, y + 6], 90, 270, fill=1, width=2)  # handles
    d.arc([x + 5, y - 2, x + 13, y + 6], 270, 90, fill=1, width=2)
    d.rectangle([x - 1, y + 10, x + 1, y + 14], fill=1)              # stem
    d.rectangle([x - 6, y + 14, x + 6, y + 16], fill=1)              # base


def a_trophy():
    """Football: lifts the trophy (a big milestone, a release)."""
    out = []
    for i in steps(24):
        lift = pick([0, -2, -4, -2], i)

        def ex(d, i=i, lift=lift):
            trophy(d, 64, 6 + lift)
            for (sx, sy) in ([(20, 10), (108, 14), (14, 40), (112, 44)] if i % 2 else [(26, 20), (102, 8), (20, 52), (106, 34)]):
                d.line([(sx - 2, sy), (sx + 2, sy)], fill=1)
                d.line([(sx, sy - 2), (sx, sy + 2)], fill=1)
        out.append((sprite_frame(46, 38 + lift // 2, arms="up", eyes="wink" if i in (10, 11) else "open", extras=[ex]), ms(90)))
    return out


def a_scoreboard():
    """Match mode: tasks done vs bugs, the score ticks up when Claude finishes."""
    small = TEXT_FONT
    big = ImageFont.truetype(BOLD, 24)
    out = []

    def board(d, left, right, roll=0):
        d.rectangle([0, 0, 127, 63], outline=1)
        d.text((8, 4), "CLAUDE", fill=1, font=small)
        d.text((92, 4), "BUGS", fill=1, font=small)
        d.text((22, 22 - roll), str(left), fill=1, font=big)
        if roll:
            d.text((22, 22 - roll + 26), str(left + 1), fill=1, font=big)
        d.rectangle([60, 34, 67, 37], fill=1)
        d.text((88, 22), str(right), fill=1, font=big)
        d.rectangle([1, 50, 126, 51], fill=0)

    def masked(left, right, roll=0):
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        board(d, left, right, roll)
        d.rectangle([1, 1, 126, 18], fill=0)          # keep the rolling digit inside its window
        d.text((8, 4), "CLAUDE", fill=1, font=small)
        d.text((92, 4), "BUGS", fill=1, font=small)
        d.rectangle([1, 51, 126, 62], fill=0)
        d.text((40, 52), "62' 5h 42%", fill=1, font=small)
        return img
    out.append((masked(2, 1), 900))
    for r in range(2, 27, 4):
        out.append((masked(2, 1, r), 40))
    out.append((masked(3, 1), 300))
    for i in steps(6):
        img = masked(3, 1)
        if i % 2 == 0:
            ImageDraw.Draw(img).rectangle([18, 20, 46, 48], outline=1)
        out.append((img, ms(150)))
    return out + [(masked(3, 1), 900)]


def card(d, x, y, red):
    if red:
        d.rectangle([x, y, x + 15, y + 21], fill=1)
    else:
        d.rectangle([x, y, x + 15, y + 21], outline=1)
        for k in range(round(y) + 3, round(y) + 20, 3):
            d.line([(x + 3, k), (x + 12, k)], fill=1)


def card_anim(red):
    out = [(frame(), 300)]
    for i in steps(8):                          # the card goes up
        t = i / 7
        out.append((frame(*both(shape="rect", dy=0), extras=[lambda d, t=t: card(d, 104, 64 - 54 * ease(t), red)]), ms(50)))
    face = both(shape="sad", dy=4) if red else both(w=15, h=26, dy=-2)
    for i in range(14):
        out.append((frame(*face, extras=[lambda d, i=i: card(d, 104, 10 - (i % 4 == 0), red)]), 90))
    return out + [(frame(*face), 500)]


def a_yellow_card():
    """Match mode: a warning (lint, a failed check that is not fatal)."""
    return card_anim(red=False)


def a_red_card():
    """Match mode: tests failed or the build broke."""
    return card_anim(red=True)


def a_var_check():
    """Match mode: VAR review = Claude is waiting for your permission."""
    out = []
    for i in steps(30):
        scan = 14 + (i * 3) % 30

        def tv(d, scan=scan, i=i):
            d.rectangle([62, 8, 122, 50], outline=1)
            d.text((80, 12), "VAR", fill=1, font=ImageFont.truetype(BOLD, 12))
            d.line([(64, scan), (120, scan)], fill=1)
            d.text((70, 52), "checking" + "." * int(i // 4 % 4), fill=1, font=TEXT_FONT)
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        dx = round(3 * math.sin(i / 3))
        for cx in (16, 38):
            draw_eye(d, cx + dx, 30, Eye(w=8, h=16 if i % 12 else 2))
        tv(d)
        out.append((img, ms(80)))
    img = Image.new("1", (W, H), 0)
    d = ImageDraw.Draw(img)
    for cx in (16, 38):
        draw_eye(d, cx, 30, Eye(w=8, h=16))
    d.rectangle([62, 8, 122, 50], outline=1)
    d.line([(78, 30), (88, 40), (106, 18)], fill=1, width=3)       # tick
    return out + [(img, 900)]


def a_substitution():
    """Match mode: model switch (Sonnet off, Opus on) or a subagent joins."""
    out = []
    for i in steps(24):
        def board(d, i=i):
            d.rectangle([24, 4, 104, 44], outline=1)
            d.text((34, 8), "OUT", fill=1, font=TEXT_FONT)
            d.text((60, 8), "SONNET", fill=1, font=TEXT_FONT)
            d.text((34, 26), "IN", fill=1, font=TEXT_FONT)
            d.text((60, 26), "OPUS", fill=1, font=TEXT_FONT)
            if i % 4 < 2:
                d.polygon([(28, 12), (31, 16), (25, 16)], fill=1)    # up arrow (in)
                d.polygon([(25, 28), (31, 28), (28, 32)], fill=1)    # down arrow (out)
            d.rectangle([62, 44, 66, 52], fill=1)                    # handle
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        board(d)
        for cx in (16, 112):
            draw_eye(d, cx, 54, Eye(w=6, h=10 if i % 10 else 2))
        out.append((img, ms(90)))
    return out


def a_stoppage_time():
    """Match mode: the 5 hour limit is nearly used, '+3' board goes up."""
    big = ImageFont.truetype(BOLD, 21)
    out = []
    for i in steps(10):
        t = i / 9
        out.append((frame(*both(dy=4, lid=0.2, dx=-14), extras=[lambda d, t=t: (d.rectangle([80, 64 - 60 * ease(t), 126, 64 - 60 * ease(t) + 34], outline=1),
                                                                        d.text((86, 64 - 60 * ease(t) + 5), "+3", fill=1, font=big))]), ms(50)))
    for i in steps(16):
        on = i % 4 < 3
        out.append((frame(*both(dy=4, lid=0.2, dx=-14), extras=[lambda d, on=on: (d.rectangle([80, 4, 126, 38], outline=1),
                                                                               d.text((86, 9), "+3", fill=1, font=big) if on else None)]), ms(110)))
    return out


def a_full_time():
    """Match mode: the usage limit reset, full-time whistle, happy."""
    out = []
    for i in steps(20):
        def ex(d, i=i):
            d.text((34, 0), "FULL TIME", fill=1, font=TEXT_FONT)
            d.ellipse([104, 50, 112, 58], outline=1, width=2)         # whistle, bottom right
            d.rectangle([110, 51, 120, 54], fill=1)
            for k in range(3):
                if int(i + k) % 3 == 0:
                    d.arc([114 + k * 3, 38 - k * 4, 124 + k * 3, 48 - k * 4], 200, 330, fill=1)
        out.append((frame(*both(shape="happy", dy=pick([0, -2, -3, -2], i) + 4), extras=[ex]), ms(90)))
    return out



# ------------------------------------------------------------- subagents

SQUAD = [(10, 8), (37, 8), (64, 8), (91, 8), (23, 36), (50, 36), (77, 36), (104, 36)]   # 8 slots


def a_subagents():
    """Subagents: a half-size Clawd pops in for each one, they bounce, then pass a ball."""
    out = []
    n = len(SQUAD)
    for k in range(1, n + 1):                   # one by one, with a pop
        for pop in (2, 0):
            img = Image.new("1", (W, H), 0)
            d = ImageDraw.Draw(img)
            for j in range(k):
                x, y = SQUAD[j]
                cs.draw(d, x, y - (pop if j == k - 1 else 0), scale=0.5)
            d.text((112, 52), f"x{k}", fill=1, font=TEXT_FONT) if k < 10 else None
            out.append((img, 70 if pop else 110))
    order = [0, 5, 2, 7, 4, 1, 6, 3, 0]         # passing the ball around the squad
    for a, b in zip(order, order[1:]):
        (x0, y0), (x1, y1) = SQUAD[a], SQUAD[b]
        for i in range(10):
            t = i / 9
            img = Image.new("1", (W, H), 0)
            d = ImageDraw.Draw(img)
            for j, (x, y) in enumerate(SQUAD):
                hop = round(1.5 * (1 + math.sin(i / 9 * 2 * math.pi + j)))     # everyone bobs
                kicker = j == a and i < 2
                cs.draw(d, x, y - hop, eyes="open", feet="left" if kicker else "both", scale=0.5)
            bx = x0 + 9 + (x1 - x0) * t
            by = y0 + 11 + (y1 - y0) * t - 10 * math.sin(t * math.pi)
            ball(d, bx, by, 2)
            out.append((img, 45))
    return out


def a_subagents_march():
    """Subagents: the squad marches across the screen like a team walking out."""
    out = []
    n = 6
    for i in steps(48):
        img = Image.new("1", (W, H), 0)
        d = ImageDraw.Draw(img)
        d.line([(0, 63), (127, 63)], fill=1)
        for j in range(n):
            x = -22 - j * 24 + i * 4
            if -20 < x < W:
                step = (i + j) % 2
                cs.draw(d, x, 46 - (1 if step else 0), feet="left" if step else "right", scale=0.5)
        d.text((2, 2), f"{n} subagents", fill=1, font=TEXT_FONT)
        out.append((img, ms(50)))
    return out

ANIMATIONS = {
    # name: (function, group, where it comes from, when the pet plays it)
    "blink": (a_blink, "Claude Code", "Claude Code", "idle, all the time"),
    "look_around": (a_look_around, "Claude Code", "Claude Code", "idle, now and then"),
    "wink": (a_wink, "Claude Code", "Claude Code", "session started"),
    "turn_around": (a_turn_around, "Claude Code", "Claude Code", "celebration"),
    "thinking": (a_thinking, "Claude Code", "Claude Code spinner", "you sent a prompt"),
    "spin": (a_spin, "Claude Code", "Claude Code turn frames", "tapped twice"),
    "wave": (a_wave, "Claude Code", "Claude Code arm poses", "you sit down / first session"),
    "keepy_uppy": (a_keepy_uppy, "Football", "new", "long idle, just for fun"),
    "goal": (a_goal, "Football", "new", "a big task finished"),
    "ball_watch": (a_ball_watch, "Football", "new", "idle, now and then"),
    "penalty_save": (a_penalty_save, "Football", "new", "you stopped Claude just in time (Esc)"),
    "header": (a_header, "Football", "new", "a quick task finished"),
    "bicycle_kick": (a_bicycle_kick, "Football", "new", "a hard task finished first try"),
    "dribble": (a_dribble, "Football", "new", "Claude is editing many files"),
    "knee_slide": (a_knee_slide, "Football", "new", "a PR merged"),
    "trophy": (a_trophy, "Football", "new", "a release / a big milestone"),
    "subagents": (a_subagents, "Subagents", "new", "one mini Clawd per running subagent, passing the ball"),
    "subagents_march": (a_subagents_march, "Subagents", "new", "a batch of subagents starts"),
    "scoreboard": (a_scoreboard, "Match mode", "new", "your session as a match: tasks done vs bugs"),
    "yellow_card": (a_yellow_card, "Match mode", "new", "a warning, lint failed"),
    "red_card": (a_red_card, "Match mode", "new", "tests failed / build broke"),
    "var_check": (a_var_check, "Match mode", "new", "Claude is waiting for your permission"),
    "substitution": (a_substitution, "Match mode", "new", "model switched, or a subagent joins"),
    "stoppage_time": (a_stoppage_time, "Match mode", "new", "5 hour limit nearly used"),
    "full_time": (a_full_time, "Match mode", "new", "usage limit reset"),
    "petted": (a_petted, "> <", "new", "touch pad on the head"),
    "giggle": (a_giggle, "> <", "new", "tapped quickly a few times"),
    "shy": (a_shy, "> <", "new", "you say thanks to Claude"),
    "laugh": (a_laugh, "> <", "new", "a joke lands / long touch"),
    "working": (a_working, "Working", "new", "Claude is running tools"),
    "searching": (a_searching, "Working", "new", "Grep / Glob running"),
    "needs_you": (a_needs_you, "Working", "new", "Claude is waiting for permission"),
    "done": (a_done, "Working", "new", "Claude finished"),
    "party": (a_party, "Working", "new", "git commit / PR merged"),
    "sleepy": (a_sleepy, "Idle", "new", "no session for a while"),
    "wake_up": (a_wake_up, "Idle", "new", "first prompt after sleeping"),
    "peek": (a_peek, "Idle", "new", "idle, now and then"),
    "music": (a_music, "Idle", "new", "session open, nothing running"),
    "surprised": (a_surprised, "Moods", "new", "an interrupt"),
    "sad": (a_sad, "Moods", "new", "a task failed"),
    "angry": (a_angry, "Moods", "new", "tests failing again"),
    "dizzy": (a_dizzy, "Moods", "new", "many retries in a row"),
    "love": (a_love, "Moods", "new", "touch held for 2 seconds"),
    "tired": (a_tired, "Usage", "new", "a usage limit above 80%"),
    "limit_hit": (a_limit_hit, "Usage", "new", "a usage limit reached"),
    "usage": (a_usage, "Usage", "new", "touch to show usage"),
}


# ---------------------------------------------------------------- output

def to_rgb(img):
    """Upscale a 1-bit frame with an OLED look (small gaps between pixels)."""
    big = Image.new("RGB", (W * SCALE, H * SCALE), OFF)
    d = ImageDraw.Draw(big)
    px = img.load()
    for y in range(H):
        for x in range(W):
            if px[x, y]:
                d.rectangle([x * SCALE, y * SCALE, x * SCALE + SCALE - 2, y * SCALE + SCALE - 2], fill=ON)
    return big


def save_gif(name, frames):
    imgs = [to_rgb(f) for f, _ in frames]
    durations = [ms for _, ms in frames]
    imgs[0].save(OUT / f"{name}.gif", save_all=True, append_images=imgs[1:],
                 duration=durations, loop=0, disposal=2, optimize=False)


def gallery():
    groups = {}
    for n, (_, group, src, when) in ANIMATIONS.items():
        groups.setdefault(group, []).append(
            f'<div class="card"><div class="face"><img src="{n}.gif"></div>'
            f'<h3>{n.replace("_", " ")}</h3><p><span>{src}</span> {when}</p></div>')
    sections = "\n".join(f'<h2>{g}</h2><div class="grid">{"".join(c)}</div>' for g, c in groups.items())
    (OUT / "gallery.html").write_text(f"""<!doctype html><html><head><meta charset="utf-8">
<title>Clawd eyes</title><style>
body {{ background: #1b1d22; color: #ddd; font: 14px sans-serif; margin: 24px; }}
h1 {{ font-weight: 600; }} h2 {{ margin: 34px 0 12px; color: #e8a07a; font-weight: 600; }}
.grid {{ display: flex; flex-wrap: wrap; gap: 22px; }}
.card {{ width: 340px; }} .face {{ background: #c8372d; padding: 26px 22px; border-radius: 6px; }}
.face img {{ width: 100%; display: block; border: 3px solid #5a1712; image-rendering: pixelated; }}
h3 {{ margin: 10px 0 2px; color: #fff; }} p {{ margin: 0; color: #aaa; }}
span {{ color: #e8a07a; font-size: 12px; margin-right: 6px; }}
</style></head><body><h1>Clawd eyes, 128x64 OLED ({len(ANIMATIONS)} animations)</h1>
<p>Red frame = the printed face. Label = where the animation comes from, then when the pet plays it.</p>
{sections}</body></html>""")


def main():
    OUT.mkdir(exist_ok=True)
    names = sys.argv[1:] or list(ANIMATIONS)
    for n in names:
        frames = ANIMATIONS[n][0]()
        save_gif(n, frames)
        print(f"{n}: {len(frames)} frames, {sum(ms for _, ms in frames) / 1000:.1f} s")
    gallery()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Turns out/*.frames from the simulator into gifs with an OLED look.

  python3 gif.py out/blink.frames [more...]     -> out/blink.gif
  python3 gif.py --png 40 out/blink.frames      -> one frame as png (frame index)
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

W, H, SCALE = 128, 64, 4
FRAME_MS = 20
ON, OFF = (232, 240, 255), (10, 12, 16)


def frames(path):
    data = Path(path).read_bytes()
    n = len(data) // 1024
    for i in range(n):
        yield data[i * 1024:(i + 1) * 1024]


def to_img(buf):
    big = Image.new("RGB", (W * SCALE, H * SCALE), OFF)
    d = ImageDraw.Draw(big)
    for page in range(8):
        for x in range(W):
            b = buf[page * W + x]
            if not b:
                continue
            for bit in range(8):
                if b >> bit & 1:
                    y = page * 8 + bit
                    d.rectangle([x * SCALE, y * SCALE, x * SCALE + SCALE - 2, y * SCALE + SCALE - 2], fill=ON)
    return big


def load_captions(path):
    """out/x.captions from `sim script`: frame index -> (event, anim) shown from then on."""
    cap = Path(path).with_suffix(".captions")
    if not cap.exists():
        return None
    marks = []
    for line in cap.read_text().splitlines():
        idx, kind, text = line.split(" ", 2)
        marks.append((int(idx), kind, text))
    return marks


def with_caption(img, event, anim):
    font = ImageFont.truetype("DejaVuSansMono.ttf", 15)
    out = Image.new("RGB", (img.width, img.height + 34), (24, 26, 32))
    out.paste(img, (0, 0))
    d = ImageDraw.Draw(out)
    d.text((10, img.height + 8), event, fill=(232, 160, 122), font=font)
    d.text((img.width - 10 - d.textlength(anim, font=font), img.height + 8), anim, fill=(170, 180, 200), font=font)
    return out


def save_gif(path):
    marks = load_captions(path)
    event, anim, mi = "", "", 0
    imgs, durs, last = [], [], None
    for i, buf in enumerate(frames(path)):
        while marks and mi < len(marks) and marks[mi][0] <= i:
            _, kind, text = marks[mi]
            if kind == "e":
                event = "> " + text
            else:
                anim = text
            mi += 1
        key = (buf, event, anim)
        if key == last:
            durs[-1] += FRAME_MS
            continue
        img = to_img(buf)
        imgs.append(with_caption(img, event, anim) if marks is not None else img)
        durs.append(FRAME_MS)
        last = key
    if not imgs:
        return
    out = Path(path).with_suffix(".gif")
    imgs[0].save(out, save_all=True, append_images=imgs[1:], duration=durs, loop=0, disposal=2, optimize=False)
    print(f"{out}: {len(imgs)} frames, {sum(durs) / 1000:.1f} s")


def main():
    args = sys.argv[1:]
    if args and args[0] == "--png":
        idx = int(args[1])
        for p in args[2:]:
            buf = list(frames(p))[idx]
            out = Path(p).with_name(f"{Path(p).stem}_{idx}.png")
            to_img(buf).save(out)
            print(out)
        return
    for p in args:
        save_gif(p)


if __name__ == "__main__":
    main()

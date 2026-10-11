#!/usr/bin/env python3
"""Turns out/*.frames from the simulator into gifs with an OLED look.

  python3 gif.py out/blink.frames [more...]     -> out/blink.gif
  python3 gif.py --png 40 out/blink.frames      -> one frame as png (frame index)
"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

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


def save_gif(path):
    imgs, durs, last = [], [], None
    for buf in frames(path):
        if buf == last:
            durs[-1] += FRAME_MS
            continue
        imgs.append(to_img(buf))
        durs.append(FRAME_MS)
        last = buf
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

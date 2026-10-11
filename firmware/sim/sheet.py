#!/usr/bin/env python3
"""Contact sheet of a .frames file: python3 sheet.py out/demo.frames start_s end_s step_s"""
import sys
from pathlib import Path

from PIL import Image, ImageDraw

W, H = 128, 64


def main():
    path, a, b, step = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
    data = Path(path).read_bytes()
    times = []
    t = a
    while t <= b:
        times.append(t)
        t += step
    cols = 6
    rows = (len(times) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (W * 2 + 6), rows * (H * 2 + 18)), (60, 20, 20))
    d = ImageDraw.Draw(sheet)
    for k, t in enumerate(times):
        i = int(t * 50)
        buf = data[i * 1024:(i + 1) * 1024]
        if len(buf) < 1024:
            break
        img = Image.new("1", (W, H))
        px = img.load()
        for p in range(8):
            for x in range(W):
                for bit in range(8):
                    if buf[p * W + x] >> bit & 1:
                        px[x, p * 8 + bit] = 1
        x0, y0 = (k % cols) * (W * 2 + 6), (k // cols) * (H * 2 + 18)
        sheet.paste(img.convert("RGB").resize((W * 2, H * 2)), (x0, y0 + 16))
        d.text((x0 + 2, y0 + 2), f"{t:.2f}s", fill=(255, 220, 200))
    out = Path(path).with_name(Path(path).stem + "_sheet.png")
    sheet.save(out)
    print(out)


if __name__ == "__main__":
    main()

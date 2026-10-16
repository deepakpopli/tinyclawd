"""Simple model of the electronics inside Clawd, for renders only (not printed).
ESP32-C3 Super Mini with its USB-C in the back plate opening, the OLED module
behind the screen, the TTP223 pad under the touch pocket, and the wires.

Plain python, no numpy, so Blender can use it too.
Housing coords in mm: x = width, y = depth (0 = front, back plate inner face at 42.5),
z = height (0 = original bottom).

  parts() -> [(name, color, metal, rough, pieces)], pieces = [(verts, faces)], each piece is
  one closed solid. faces are quads or convex caps, winding is not fixed: recalc normals per piece."""
import math

# USB-C opening in the back plate: x 13.5..32.5, z 4.75..12.5, centre x 23, z 8.6
USB_X, USB_Z = 23.0, 8.6
PLATE_Y = 42.5

PCB_Z = (5.4, 7.0)                   # ESP board, 1.6 mm, USB-C sits on top
PCB_Y = (18.8, 41.3)                 # 22.5 long, the USB-C sticks out 1 mm at the back
PCB_X = (USB_X - 9, USB_X + 9)       # 18 wide
TOP = PCB_Z[1]
PAD_PITCH = 2.54


def pad_y(i):
    # 8 pads a side, i = 0 next to the USB-C
    return (PCB_Y[0] + PCB_Y[1]) / 2 + (3.5 - i) * PAD_PITCH


class Part:
    def __init__(self, name, color, metal=0.0, rough=0.5):
        self.name, self.color, self.metal, self.rough = name, color, metal, rough
        self.pieces = []

    def add(self, verts, faces):
        self.pieces.append((verts, faces))

    def box(self, x0, x1, y0, y1, z0, z1):
        v = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
        f = [[0, 1, 3, 2], [4, 6, 7, 5], [0, 4, 5, 1], [2, 3, 7, 6], [0, 2, 6, 4], [1, 5, 7, 3]]
        self.add(v, f)

    def cbox(self, cx, cy, cz, sx, sy, sz):
        self.box(cx - sx / 2, cx + sx / 2, cy - sy / 2, cy + sy / 2, cz - sz / 2, cz + sz / 2)

    def tube(self, pts, r, sides=6):
        # round wire along a path, closed at both ends
        rings = []
        u = None
        for i, p in enumerate(pts):
            a, b = pts[max(i - 1, 0)], pts[min(i + 1, len(pts) - 1)]
            t = norm(sub(b, a))
            if u is None:
                u = norm(cross(t, (1.0, 0.0, 0.0) if abs(t[0]) < 0.9 else (0.0, 0.0, 1.0)))
            u = norm(sub(u, mul(t, dot(u, t))))     # carry the ring along without twisting
            w = cross(t, u)
            rings.append([add3(p, add3(mul(u, r * math.cos(k * 2 * math.pi / sides)),
                                       mul(w, r * math.sin(k * 2 * math.pi / sides))))
                          for k in range(sides)])
        v = [q for ring in rings for q in ring]
        f = []
        for i in range(len(rings) - 1):
            for k in range(sides):
                k2 = (k + 1) % sides
                f.append([i * sides + k, i * sides + k2, (i + 1) * sides + k2, (i + 1) * sides + k])
        f.append(list(range(sides)))
        f.append([(len(rings) - 1) * sides + k for k in range(sides)])
        self.add(v, f)


def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add3(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    n = math.sqrt(dot(a, a)) or 1.0
    return mul(a, 1 / n)


def bezier(p0, p1, p2, p3, n=14):
    out = []
    for i in range(n + 1):
        t = i / n
        s = 1 - t
        out.append(tuple(s ** 3 * a + 3 * s * s * t * b + 3 * s * t * t * c + t ** 3 * d
                         for a, b, c, d in zip(p0, p1, p2, p3)))
    return out


def stadium(cx, cz, w, h, n=8):
    # rounded slot outline in x-z, half circles at the ends
    r = h / 2
    pts = []
    for x, start in ((cx + w / 2 - r, -math.pi / 2), (cx - w / 2 + r, math.pi / 2)):
        for k in range(n + 1):
            a = start + math.pi * k / n
            pts.append((x + r * math.cos(a), cz + r * math.sin(a)))
    return pts


def usb_c(metal, dark, gold):
    # shell 9 x 3.2, 7.3 deep, mouth 0.2 mm inside the back plate
    y_mouth, y_back, wall = PLATE_Y - 0.2, PLATE_Y - 0.2 - 7.3, 0.3
    cz = TOP + 1.6
    out = stadium(USB_X, cz, 9.0, 3.2)
    inn = stadium(USB_X, cz, 9.0 - 2 * wall, 3.2 - 2 * wall)
    n = len(out)
    v = [(x, y, z) for y, ring in ((y_back, out), (y_mouth, out), (y_mouth, inn), (y_back + wall, inn))
         for x, z in ring]
    f = []
    for ring in range(3):
        for k in range(n):
            k2 = (k + 1) % n
            f.append([ring * n + k, ring * n + k2, (ring + 1) * n + k2, (ring + 1) * n + k])
    f.append(list(range(n)))                       # back of the shell
    f.append([3 * n + k for k in range(n)])        # inside back wall
    metal.add(v, f)
    # black tongue with the contacts on top
    dark.box(USB_X - 3.35, USB_X + 3.35, y_back + 1.0, y_mouth - 0.7, cz - 0.35, cz + 0.35)
    dark.box(USB_X - 4.1, USB_X + 4.1, y_back + wall, y_back + 0.9, cz - 1.1, cz + 1.1)
    for k in range(6):
        x = USB_X - 2.5 + k
        gold.box(x - 0.18, x + 0.18, y_mouth - 3.2, y_mouth - 0.8, cz + 0.35, cz + 0.4)


def parts():
    pcb = Part("pcb", "#141518", rough=0.35)
    metal = Part("metal", "#c4c8cc", metal=1.0, rough=0.28)
    gold = Part("gold", "#d6a43a", metal=1.0, rough=0.3)
    dark = Part("dark", "#0b0b0c", rough=0.45)
    smd = Part("smd", "#a88a5c", rough=0.5)              # small capacitors
    ceramic = Part("ceramic", "#e4dfd2", rough=0.45)     # chip antenna
    oled = Part("oled_pcb", "#1a1c20", rough=0.4)
    touch = Part("touch_pcb", "#b3202a", rough=0.4)
    wires = {c: Part("wire_" + c, hexc, rough=0.55) for c, hexc in
             (("red", "#d32f2f"), ("black", "#1c1c1e"), ("yellow", "#f0c02c"),
              ("blue", "#2f6fd6"), ("green", "#2e9e4f"))}

    # ---- ESP32-C3 Super Mini
    pcb.box(*PCB_X, *PCB_Y, *PCB_Z)
    usb_c(metal, dark, gold)
    for x in (PCB_X[0] + 2.7, PCB_X[1] - 2.7):           # BOOT and RST, beside the USB-C
        metal.box(x - 1.4, x + 1.4, 37.4, 40.8, TOP, TOP + 1.5)
        dark.cbox(x, 39.1, TOP + 1.7, 1.4, 1.8, 0.4)
    metal.box(USB_X - 2.5, USB_X + 2.5, 26.5, 31.5, TOP, TOP + 0.85)     # ESP32-C3
    metal.box(26.8, 28.8, 27.5, 30.0, TOP, TOP + 0.6)                     # crystal
    dark.box(USB_X - 1.5, USB_X + 1.5, 32.8, 34.4, TOP, TOP + 1.0)        # regulator
    for x, y, turn in ((17.0, 31.0, 0), (17.0, 28.5, 0), (29.6, 32.6, 1), (19.4, 24.2, 1),
                       (26.8, 24.4, 0), (25.8, 33.6, 1), (20.0, 33.6, 1)):
        sx, sy = (1.6, 0.8) if turn else (0.8, 1.6)
        smd.cbox(x, y, TOP + 0.4, sx, sy, 0.8)
    ceramic.box(USB_X - 4.5, USB_X + 4.5, 19.4, 21.2, TOP, TOP + 1.0)    # antenna
    for x in (USB_X - 4.5, USB_X + 4.5):
        gold.cbox(x, 20.3, TOP + 0.52, 0.6, 2.0, 1.05)
    for i in range(8):                                     # castellated pads
        for x in (PCB_X[0] + 0.6, PCB_X[1] - 0.6):
            gold.cbox(x, pad_y(i), TOP + 0.03, 1.2, 1.2, 0.06)

    # ---- OLED module behind the screen, header at the top edge pointing back
    dark.box(6.0, 40.0, 2.6, 3.9, 13.0, 32.5)              # glass panel body
    oled.box(5.3, 40.7, 3.9, 5.1, 6.0, 39.0)
    dark.box(USB_X - 5.08, USB_X + 5.08, 5.1, 7.6, 36.0, 38.5)
    oled_pins = [USB_X - 3.81 + k * PAD_PITCH for k in range(4)]   # GND VCC SCL SDA
    for x in oled_pins:
        metal.box(x - 0.32, x + 0.32, 3.3, 11.0, 36.93, 37.57)

    # ---- TTP223 pad under the touch pocket, pins down
    touch.box(17.5, 28.5, 27.2, 41.2, 38.0, 39.2)
    dark.box(21.5, 24.5, 33.0, 35.0, 37.2, 38.0)
    dark.box(19.2, 26.8, 27.2, 29.6, 35.5, 38.0)
    touch_pins = [USB_X - PAD_PITCH + k * PAD_PITCH for k in range(3)]   # GND SIG VCC
    for x in touch_pins:
        metal.box(x - 0.32, x + 0.32, 28.08, 28.72, 33.5, 38.0)

    # ---- wires, soldered on the pads from above
    left, right = PCB_X[0] + 0.6, PCB_X[1] - 0.6
    end_z = TOP + 0.35

    def from_oled(k, pad, color):
        x, y = pad
        wires[color].tube(bezier((oled_pins[k], 10.6, 37.25), (oled_pins[k], 20.0, 37.25),
                                 (x, y, 24.0), (x, y, end_z)), 0.55)

    def from_touch(k, pad, color):
        x, y = pad
        wires[color].tube(bezier((touch_pins[k], 28.4, 33.6), (touch_pins[k], 28.4, 24.0),
                                 (x, y, 20.0), (x, y, end_z)), 0.55)

    from_oled(0, (left - 0.25, pad_y(1)), "black")        # GND
    from_oled(1, (left - 0.25, pad_y(2)), "red")          # 3V3
    from_oled(2, (right, pad_y(7)), "yellow")             # SCL GPIO21
    from_oled(3, (right, pad_y(6)), "blue")               # SDA GPIO20
    from_touch(0, (left + 0.3, pad_y(1)), "black")        # GND
    from_touch(1, (left, pad_y(6)), "green")              # SIG GPIO1
    from_touch(2, (left + 0.3, pad_y(2)), "red")          # 3V3

    out = [pcb, metal, gold, dark, smd, ceramic, oled, touch, *wires.values()]
    return [(p.name, p.color, p.metal, p.rough, p.pieces) for p in out]


if __name__ == "__main__":
    for name, color, *_, pieces in parts():
        print(f"{name:12} {color} {len(pieces):3} pieces {sum(len(v) for v, f in pieces):5} verts")

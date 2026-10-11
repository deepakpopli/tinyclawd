# AGENTS.md

notes for anyone (or any coding agent) working in this repo.

## layout

```
firmware/     esp-idf app for the ESP32-C3 (pet engine, display, touch, usb link)
bridge/       laptop side: claude code status line + hooks -> usb serial -> pet
animations/   eyes.py = reference design of every animation (1-bit 128x64), gifs/ = previews
hardware/     openscad designs, fit/print checks, build.sh, print/ = files to print
```

## hardware

| part | detail |
|---|---|
| mcu | ESP32-C3 Super Mini, 4 MB flash, USB-Serial-JTAG on /dev/ttyACM0 |
| display | SH1106 128x64, I2C 0x3C, SDA GPIO20, SCL GPIO21, column offset 2 (132-wide ram) |
| touch | TTP223 on GPIO1, active high |
| other | no buzzer, no battery, usb-c power. onboard antenna is weak |

## firmware

esp-idf v5.4.x, C++.

```bash
. ~/esp/esp-idf/export.sh
idf.py -C firmware build
idf.py -C firmware -p /dev/ttyACM0 flash monitor
```

keep the pet engine (state + animations) free of esp-idf headers so it also builds on the desktop
simulator and can be tested without the board.

## animations

`animations/eyes.py` is the spec. every animation is plain shapes (rects, lines, small glyphs) on a
1-bit 128x64 canvas, so it ports 1:1 to C++. Clawd's eyes are solid slits, no pupils.
`animations/clawd_sprite.py` holds the mini Clawd block art and its 360 turn, taken from the
Claude Code banner.

## hardware files

`hardware/build.sh` rebuilds everything and runs the checks (`check_fit.py`, `check_stands.py`).
any change to a housing must keep `check_fit.py` passing: the inside has to stay identical to the
original Mochi frame. 3mf parts are mapped to AMS slots in `print_3mf.py`.

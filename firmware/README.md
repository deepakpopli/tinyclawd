# firmware

esp-idf v5.4 app for the ESP32-C3 Super Mini. the pet engine lives in `components/pet`
(no esp-idf headers, the desktop sim in `sim/` builds the same code). `main/` is the board part:
screen, touch, usb link and the render loop.

## build + flash

```bash
. ~/esp/esp-idf/export.sh
cd firmware
idf.py set-target esp32c3        # only the first time
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

on a busy laptop: `nice -n 19 idf.py reconfigure && nice -n 19 ninja -C build -j2`.

`sdkconfig` is generated from `sdkconfig.defaults` and not in git. if you change a default,
delete `sdkconfig` and run `idf.py reconfigure`.

## pins

| what | pin |
|---|---|
| OLED SDA | GPIO 20 |
| OLED SCL | GPIO 21 |
| touch (TTP223, high while touched) | GPIO 1 |

screen is an SH1106 at 0x3C, the 128 visible columns start at ram column 2.

## tasks

| task | prio | what |
|---|---|---|
| render | 5 | owns the Brain. every 25 ms (40 fps): reads queued lines and touches, draws, sends changed pages |
| touch | 6 | polls GPIO 1 every 10 ms, 30 ms debounce, pushes tap / double / long |
| link | 4 | reads the usb serial port, builds lines (max 120 chars), pushes them to a queue |

only the render task calls the Brain. the other two just fill queues. with no laptop the link
task sits idle and the pet lives on its own. replies are dropped if nobody reads the port.

console logs share the same usb port, so the log level is WARN. lines from the pet are the
ones in `PROTOCOL.md`, anything starting with `E (` or `W (` is a log.

flash layout (`partitions.csv`): nvs, otadata, phy_init and two 1.8 MB app slots, so OTA can be
added later without repartitioning. wifi and ota are not in v1.

## check on the real board

- screen: picture is not shifted (offset 2), not mirrored, no noise. if it is, check `COL_OFFSET`
  and the 0xA1 / 0xC8 lines in `sh1106.cpp`.
- I2C speed: 400 kHz now (`I2C_HZ` in `sh1106.cpp`). a full frame takes ~23 ms at 400k, most
  frames only send a few pages. try 800 kHz if animations look slow, go back if you see glitches.
- brightness: contrast is 0xCF at boot, `sh1106::set_contrast()` changes it.
- touch feel: `DOUBLE_MS` (280), `LONG_MS` (700) and `DEBOUNCE_MS` (30) in `touch.cpp`. a single
  tap only fires after the double tap window, so it feels a bit late on purpose.
- link: `echo "hello 1" > /dev/ttyACM0` or the bridge, the pet answers `ok tinyclawd 1`.

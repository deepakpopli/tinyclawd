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

## first time on the real board

flash once, then tune everything over usb, no rebuilds. close `idf.py monitor` first, then:

```bash
python3 ../bridge/tinyclawd.py console      # type lines, see what the pet answers
```

1. `hw` shows the current settings. `screen=missing`? try `hw scan`, the screen should be at 0x3C.
2. `hw test 1`: the 1 px border must sit right on the edge on all four sides and the arrow must
   point up. border cut on one side: `hw offset 0` .. `hw offset 4`. upside down: `hw flip 1`.
   TL and TR swapped: `hw mirror 1`.
3. `hw test 2`: every pixel on, look for dead ones. pick a brightness with `hw contrast 0..255`.
4. `hw test 3`: a checker that changes every page, the worst case for I2C. wait 5 s, then
   `hw stats`. flush is the time one frame takes to send. over ~20 ms at 400k, try `hw i2c 800`
   and watch for noise. `hw test 0` goes back to the pet, `hw stats` again for normal frames.
5. touch: `hw touchraw 1` prints `raw 1` / `raw 0` on every edge. tap, double tap, long press and
   check you get `touch tap` / `touch double` / `touch long`. too slow or too strict:
   `hw double 200..400`, `hw long 500..1000`. `hw touchraw 0` when done.
6. `hw save` keeps it all for the next boot. `hw defaults` goes back to the built-in values.

the built-in defaults are offset 2, no flip, contrast 0xCF, 400 kHz, double 280 ms, long 700 ms.

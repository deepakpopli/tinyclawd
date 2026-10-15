#!/bin/bash
# builds one .bin with everything in it (bootloader, partitions, app), flashed at 0x0.
# people can flash it from the browser: https://espressif.github.io/esptool-js/
set -e
cd "$(dirname "$0")/.."
. ~/esp/esp-idf/export.sh >/dev/null
idf.py build >/dev/null
mkdir -p release
VER=${VER:-$(git describe --tags --always --dirty)}   # or VER=v1.0.0 tools/release.sh
OUT=release/tinyclawd-esp32c3-$VER.bin
idf.py merge-bin -o "$PWD/$OUT" >/dev/null
echo "$OUT ($(du -h "$OUT" | cut -f1)), flash it at 0x0"

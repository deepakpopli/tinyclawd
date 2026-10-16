#!/bin/bash
# tiny clawd, one command install (linux):
#   curl -fsSL https://tinyclawd.deepakpopli.in/install.sh | bash
# gets the code, the python bits, can flash the board, then hooks it into claude code.
set -e

REPO=${TINYCLAWD_REPO:-https://github.com/deepakpopli/tinyclawd}
DIR=${TINYCLAWD_DIR:-$HOME/.tinyclawd}
API=https://api.github.com/repos/deepakpopli/tinyclawd/releases/latest

say() { printf '\033[1;31m>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m!\033[0m %s\n' "$*"; exit 1; }
ask() { # ask "question" -> 0 for yes, works under curl | bash
    { read -r -p "$1 [y/N] " a; } 2>/dev/null </dev/tty || return 1
    [ "$a" = y ] || [ "$a" = Y ]
}

[ "$(uname -s)" = Linux ] || die "linux only for now (mac and windows later)"
command -v python3 >/dev/null || die "needs python3"
command -v git >/dev/null || die "needs git"

say "getting tiny clawd into $DIR"
if [ -d "$DIR/.git" ]; then
    git -C "$DIR" pull -q --ff-only
else
    git clone -q --depth 1 "$REPO" "$DIR"
fi

if ! python3 -c "import serial" 2>/dev/null; then
    say "installing pyserial"
    python3 -m pip install --user -q pyserial 2>/dev/null || sudo apt-get install -y python3-serial \
        || die "could not install pyserial, try: sudo apt install python3-serial"
fi

if ! id -nG | grep -qw dialout && [ ! -w /dev/ttyACM0 ]; then
    say "your user can't open usb serial ports yet. run this once, then log out and in:"
    echo "    sudo usermod -aG dialout $USER"
fi

PORT=$(ls /dev/ttyACM* 2>/dev/null | head -1 || true)
if [ -n "$PORT" ] && ask "flash the latest tiny clawd firmware to the board on $PORT? (wipes its saved settings)"; then
    python3 -c "import esptool" 2>/dev/null || python3 -m pip install --user -q esptool \
        || die "could not install esptool (pip install --user esptool)"
    URL=$(curl -fsSL "$API" | grep -o '"browser_download_url": *"[^"]*\.bin"' | head -1 | cut -d'"' -f4)
    [ -n "$URL" ] || die "no firmware .bin in the latest release"
    BIN=$(mktemp --suffix=.bin)
    curl -fsSL -o "$BIN" "$URL"
    systemctl --user stop tinyclawd-daemon 2>/dev/null || true   # it holds the port
    python3 -m esptool --chip esp32c3 -p "$PORT" write_flash 0x0 "$BIN"
    rm -f "$BIN"
fi

if [ -n "$TINYCLAWD_SKIP_SETUP" ]; then
    say "skipping setup (TINYCLAWD_SKIP_SETUP)"
else
    say "hooking it into claude code"
    python3 "$DIR/bridge/setup.py"
fi

say "done. plug Clawd in, start claude, and say hi."
say "undo: python3 $DIR/bridge/setup.py --remove && rm -rf $DIR"

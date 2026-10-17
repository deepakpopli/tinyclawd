#!/bin/bash
# tiny clawd, one command install (linux):
#   curl -fsSL https://tinyclawd.deepakpopli.in/install.sh | bash
# gets the code into ~/.tinyclawd, the python bits, can flash the board, then hooks it into
# claude code. it asks before it flashes or uses sudo. run it again to update.
#
# settings (all optional):
#   TINYCLAWD_DIR=~/somewhere   where the code goes (default ~/.tinyclawd)
#   TINYCLAWD_PORT=/dev/ttyX    the board's port, if it is not found on its own
#   TINYCLAWD_FLASH=yes|no      flash without asking (yes) or never (no)
#   TINYCLAWD_SKIP_SETUP=1      only get the code and flash, leave claude code alone

REPO=${TINYCLAWD_REPO:-https://github.com/deepakpopli/tinyclawd}
API=https://api.github.com/repos/deepakpopli/tinyclawd/releases/latest
WEB_FLASHER=https://espressif.github.io/esptool-js/
DIR=${TINYCLAWD_DIR:-$HOME/.tinyclawd}

if [ -t 1 ]; then R=$(printf '\033[1;31m'); N=$(printf '\033[0m'); else R=; N=; fi
say() { printf '%s>%s %s\n' "$R" "$N" "$*"; }
note() { printf '  %s\n' "$*"; }
die() { printf '%s!%s %s\n' "$R" "$N" "$*" >&2; exit 1; }

# a yes/no question on the terminal, also under curl | bash. no terminal = no
ask() {
    { printf '%s [y/N] ' "$1" >/dev/tty; } 2>/dev/null || return 1
    read -r a </dev/tty || return 1
    case "$a" in y|Y|yes|Yes|YES) return 0 ;; esac
    return 1
}

distro() {
    local ids=""
    [ -r /etc/os-release ] && ids=$(. /etc/os-release; echo "$ID $ID_LIKE")
    case " $ids " in
        *" arch "*) echo arch ;;
        *" fedora "*|*" rhel "*) echo fedora ;;
        *suse*) echo suse ;;
        *" debian "*|*" ubuntu "*) echo debian ;;
        *) echo other ;;
    esac
}

# package names: pkg <what>
pkg() {
    case "$(distro):$1" in
        debian:pyserial) echo python3-serial ;;
        arch:pyserial) echo python-pyserial ;;
        *:pyserial) echo python3-pyserial ;;
        arch:numpy) echo python-numpy ;;
        *:numpy) echo python3-numpy ;;
        arch:parec) echo libpulse ;;
        *:parec) echo pulseaudio-utils ;;
        arch:xinput) echo xorg-xinput ;;
        *) echo "$1" ;;
    esac
}

install_cmd() {
    case "$(distro)" in
        debian) echo "sudo apt-get install -y" ;;
        fedora) echo "sudo dnf install -y" ;;
        arch) echo "sudo pacman -S --needed --noconfirm" ;;
        suse) echo "sudo zypper install -y" ;;
        *) echo "" ;;
    esac
}

# offer to install distro packages with sudo. returns 0 if they got installed
offer_packages() {
    local why=$1; shift
    local cmd; cmd=$(install_cmd)
    [ -n "$cmd" ] || return 1
    if ask "install $why with: $cmd $*?"; then
        $cmd "$@" && return 0
    fi
    return 1
}

has_py() { "$PY" -c "import $1" 2>/dev/null; }

get_code() {
    export GIT_TERMINAL_PROMPT=0   # fail fast instead of asking for a github password
    if [ -d "$DIR/.git" ]; then
        say "updating $DIR"
        git -C "$DIR" pull -q --ff-only \
            || say "could not update $DIR (changed files in there?), carrying on with what is there"
    elif [ -e "$DIR" ] && [ -n "$(ls -A "$DIR" 2>/dev/null)" ]; then
        die "$DIR already exists and is not a tiny clawd checkout. move it, or set TINYCLAWD_DIR"
    else
        say "getting tiny clawd into $DIR"
        git clone -q --depth 1 "$REPO" "$DIR" || die "could not download $REPO"
    fi
}

get_pyserial() {
    has_py serial && return
    say "installing pyserial (the daemon talks to the board with it)"
    "$PY" -m pip install --user -q pyserial 2>/dev/null && has_py serial && return
    offer_packages pyserial "$(pkg pyserial)" && has_py serial && return
    say "no pyserial yet. the hooks still go in, but the daemon waits for it:"
    if [ -n "$(install_cmd)" ]; then
        note "$(install_cmd) $(pkg pyserial)"
    else
        note "$PY -m pip install --user pyserial"
    fi
}

extras() {
    local missing=() names=()
    command -v playerctl >/dev/null || { missing+=("$(pkg playerctl)"); names+=("song names"); }
    command -v parec >/dev/null || { missing+=("$(pkg parec)"); names+=("equalizer"); }
    command -v xinput >/dev/null || { missing+=("$(pkg xinput)"); names+=("typing"); }
    has_py numpy || { missing+=("$(pkg numpy)"); names+=("a better equalizer"); }
    [ ${#missing[@]} -eq 0 ] && return
    local list; list=$(IFS=,; echo "${names[*]}" | sed 's/,/, /g')
    say "optional extras not installed: $list"
    offer_packages "them" "${missing[@]}" || note "fine without them, those features just stay off"
}

# the board's port: TINYCLAWD_PORT, else the espressif usb device. other serial devices are
# never guessed, so a flight controller or an arduino is safe
find_port() {
    if [ -n "$TINYCLAWD_PORT" ]; then echo "$TINYCLAWD_PORT"; return; fi
    local p
    for p in /dev/serial/by-id/*Espressif* /dev/serial/by-id/*303a*; do
        [ -e "$p" ] && { readlink -f "$p"; return; }
    done
}

serial_access() {
    # $1 = port. returns 0 if we can open it now
    local port=$1 group
    [ -w "$port" ] && return 0
    group=$(stat -c %G "$port" 2>/dev/null || echo dialout)
    if id -nG | tr ' ' '\n' | grep -qx "$group"; then
        say "you are in the $group group but this login does not know yet. log out and in (or reboot)"
        return 1
    fi
    say "your user can't open $port yet, it needs the $group group"
    if ask "add $USER to $group now? (sudo usermod -aG $group $USER)"; then
        sudo usermod -aG "$group" "$USER" || return 1
        note "done. log out and back in (or reboot) so the background service can use it too"
        SG=$group   # good enough to flash right now
        return 0
    fi
    note "later: sudo usermod -aG $group $USER, then log out and in"
    return 1
}

get_esptool() {
    if has_py esptool; then ESPTOOL=("$PY" -m esptool); return 0; fi
    for e in esptool esptool.py; do
        command -v $e >/dev/null && { ESPTOOL=("$e"); return 0; }
    done
    say "installing esptool (the flashing tool from espressif)"
    if "$PY" -m pip install --user -q esptool 2>/dev/null && has_py esptool; then
        ESPTOOL=("$PY" -m esptool); return 0
    fi
    if offer_packages esptool esptool; then
        get_esptool && return 0
    fi
    return 1
}

flash() {
    local port url bin was_running=""
    port=$(find_port)
    if [ -z "$port" ]; then
        say "no board found. to flash it later: plug it in and run this again"
        note "(or use the web flasher: $WEB_FLASHER)"
        return
    fi
    case "${TINYCLAWD_FLASH:-ask}" in
        no|0) return ;;
        yes|1) ;;
        *) ask "flash the latest tiny clawd firmware to the board on $port? (replaces what is on it, saved settings and best scores reset)" || return ;;
    esac
    SG=""
    serial_access "$port" || { note "skipping the flash"; return; }
    get_esptool || { say "no esptool, skipping the flash. the web flasher works too: $WEB_FLASHER"; return; }
    url=$(curl -fsSL "$API" 2>/dev/null | grep -o '"browser_download_url": *"[^"]*\.bin"' | head -1 | cut -d'"' -f4)
    if [ -z "$url" ]; then
        say "could not find a firmware .bin in the latest github release, skipping the flash"
        return
    fi
    bin=$(mktemp --suffix=.bin)
    if ! curl -fsSL -o "$bin" "$url"; then
        rm -f "$bin"; say "download failed, skipping the flash"; return
    fi
    if systemctl --user is-active -q tinyclawd-daemon 2>/dev/null; then
        was_running=1
        systemctl --user stop tinyclawd-daemon   # it holds the port
    fi
    say "flashing $(basename "$url")"
    local cmd=("${ESPTOOL[@]}" --chip esp32c3 -p "$port" write_flash 0x0 "$bin")
    if [ -n "$SG" ]; then
        sg "$SG" -c "$(printf '%q ' "${cmd[@]}")"
    else
        "${cmd[@]}"
    fi && say "flashed" || say "flashing failed. try again, or use the web flasher: $WEB_FLASHER"
    rm -f "$bin"
    [ -n "$was_running" ] && systemctl --user start tinyclawd-daemon
}

main() {
    case "$(uname -s)" in
        Linux) ;;
        Darwin) die "the laptop side is linux only for now. you can still flash the board from chrome: $WEB_FLASHER" ;;
        *) die "linux only for now" ;;
    esac
    command -v git >/dev/null || die "needs git: $(install_cmd) git"
    command -v curl >/dev/null || die "needs curl: $(install_cmd) curl"
    PY=python3
    [ -x /usr/bin/python3 ] && PY=/usr/bin/python3   # same python the hooks use
    command -v "$PY" >/dev/null || die "needs python 3.8 or newer: $(install_cmd) python3"
    "$PY" -c 'import sys; sys.exit(sys.version_info < (3, 8))' || die "needs python 3.8 or newer"

    get_code
    get_pyserial
    extras
    flash

    if [ -n "$TINYCLAWD_SKIP_SETUP" ]; then
        say "skipping the claude code setup (TINYCLAWD_SKIP_SETUP)"
    else
        say "hooking it into claude code"
        local args=()
        [ -n "$TINYCLAWD_PORT" ] && args=(--port "$TINYCLAWD_PORT")
        "$PY" "$DIR/bridge/setup.py" "${args[@]}" | sed -e '/^undo: /d' -e '/^done\. /d' -e 's/^/  /'
        [ "${PIPESTATUS[0]}" -eq 0 ] || die "setup did not finish, see above"
    fi

    echo
    say "done. plug Clawd in, restart claude code and say hi"
    say "update: run the same command again"
    say "undo:   $PY $DIR/bridge/setup.py --remove && rm -rf $DIR"
}

main "$@"   # last line, so a cut off download runs nothing

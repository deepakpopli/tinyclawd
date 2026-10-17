# set up tiny clawd with claude code

paste this into claude code. it does the setup for you and asks before every change.

```text
Set up Tiny Clawd (https://github.com/deepakpopli/tinyclawd) on this Linux machine for me.
Before each step, say what you will do and wait for my OK. Never run sudo: show me the command and I run it.
1. Clone the repo to ~/.tinyclawd (git pull if it is already there). Read README.md and bridge/README.md.
2. Check that /usr/bin/python3 (or python3) can import serial. If not, tell me the package for my distro.
3. Ask if I want to flash the board. Only if I say yes: find it by its Espressif USB id, get the .bin from
   the latest GitHub release and flash it at 0x0 with esptool (stop tinyclawd-daemon first if it runs).
   If the port is not writable, tell me how to join the dialout group.
4. Run python3 ~/.tinyclawd/bridge/setup.py --dry-run and show me what it would change. If it says I have
   no status line, ask me which one I want: visible (shows usage), hidden (only feeds Clawd) or none.
   Then run setup.py for real, with --statusline <my answer> if you asked.
5. Test it: python3 ~/.tinyclawd/bridge/tinyclawd.py send ev done should make Clawd celebrate.
6. Tell me to restart Claude Code, and how to undo: python3 ~/.tinyclawd/bridge/setup.py --remove && rm -rf ~/.tinyclawd
```

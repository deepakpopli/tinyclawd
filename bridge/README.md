# bridge

the laptop side. claude code hooks and the status line call `tinyclawd.py`, a small daemon owns
the usb serial port and sends [protocol](../firmware/PROTOCOL.md) lines to the pet. many claude
code sessions can run at once, they all go through the same daemon.

```
claude code hooks ──┐
status line ────────┼──> unix socket ──> tinyclawd.py daemon ──> /dev/ttyACM0 ──> pet
send / replay ──────┘
```

needs python 3.8+ and pyserial (only the daemon and `replay --to serial` use it):

```bash
sudo apt install python3-serial      # fedora: python3-pyserial, arch: python-pyserial
```

## install

```bash
python3 setup.py --dry-run   # what it would change, changes nothing
python3 setup.py             # service + hooks + /clawd + usage from the status line
python3 setup.py --port /dev/ttyUSB0   # only if the pet is not found on its own
python3 setup.py --remove    # undo all of it
```

- the service is `~/.config/systemd/user/tinyclawd-daemon.service`. no systemd: it prints the
  command to run the daemon yourself
- hooks go in `~/.claude/settings.json` (or `$CLAUDE_CONFIG_DIR`). your own hooks stay where they
  are, only entries that run `tinyclawd.py` are touched. running it again fixes the paths, for
  example after you moved the folder. `install_hooks.py` does only this part
- usage: with no status line it adds a quiet one (`tinyclawd.py usage`, prints nothing). with a
  shell script status line it adds this after the line that reads stdin (`input=$(cat)`):
  ```bash
  printf '%s' "$input" | /usr/bin/python3 /path/to/bridge/tinyclawd.py usage >/dev/null 2>&1 &
  ```
  anything else, it prints that line for you to add
- settings.json and the status line script get a `.bak-tinyclawd-<time>` copy before a change.
  a bad settings.json is never touched, it tells you instead

the hooks are `async`, so claude never waits for them. `hook` and `usage` never print anything and
always exit 0, if the daemon is down the event is just dropped. the one sync hook is `screen` on
`UserPromptExpansion`: it only prints for `/clawd`, to stop that prompt before claude sees it. the
daemon is not started by the hooks, run it yourself or use the service.

## commands

```bash
python3 tinyclawd.py daemon                 # finds the pet by its espressif usb id
python3 tinyclawd.py daemon --port /dev/ttyACM1
python3 tinyclawd.py daemon --stdout        # no board, prints what it would send
python3 tinyclawd.py daemon --no-keys       # do not watch the keyboard
python3 tinyclawd.py send ev perm           # one raw line through the daemon
python3 tinyclawd.py console                # type lines straight to the pet, see its answers
python3 tinyclawd.py replay                 # plays ../firmware/sim/scripts/demo.txt
python3 tinyclawd.py replay my.txt --speed 2 --loop --to serial
```

replay goes to the daemon if it is running, else to stdout. `--to serial` opens the port itself,
so stop the daemon first. session files are one line per entry, `<ms> <line>` from the start or
`+<ms> <line>` after the previous one, `#` comments. same format as the simulator.

## what the daemon does

- sends `hello 1` and `time HH:MM` on connect, the time again every 5 min, and the last usage line
- only opens the espressif usb device (or `--port` / `TINYCLAWD_PORT`), never some other serial
  device. logs once why it can't open the port (not plugged in, no permission)
- reconnects when the board is unplugged, and says hello again when the pet prints `boot`. a write
  that hangs for 2 s counts as unplugged, so a stuck board can't freeze it
- ignores everything from the pet that is not `ok` / `err` / `touch` / `boot` (esp-idf logs),
  logs touches to stderr
- keeps state per session: subagent counts are summed over all sessions (`ev sub N`), one
  session's `ev done` or `ev end` does not hide another session that is waiting for you (`ev perm`),
  `ev end` only goes out when the last session ends, usage is only forwarded when it changed or
  every 30 s
- asks `playerctl` every 2 s if something plays (spotify, youtube in a browser...), sends
  `music 1 artist - title` when it starts or the track changes and `music 0` after 5 s of quiet.
  no playerctl, no music mode
- watches how fast you type, so the pet can look at the keyboard while you write a prompt.
  see below
- while music mode is on it also drives the equalizer, see below

## typing

the daemon runs `xinput test-xi2 --root` (x11, no root needed) and counts key presses in every
window. only the count. the key code lines come through the xinput pipe but are never parsed,
kept, logged or sent, they are dropped as they come in. the pet only gets `typing <n>`, n = keys per second (1 to 15),
and `typing 0` after 3 s without keys. at most 2 lines a second, plus the same line every 5 s
while you keep typing.

turn it off:

```bash
TINYCLAWD_NO_KEYS=1 python3 tinyclawd.py daemon
python3 tinyclawd.py daemon --no-keys
```

for the service, add `Environment=TINYCLAWD_NO_KEYS=1` under `[Service]` (or `--no-keys` to the
`ExecStart` line). on wayland, without xinput or without `DISPLAY` it is off on its own, the
daemon logs why once. the service often starts before the desktop is up, so it asks systemd for
`DISPLAY` for the first 10 minutes before it gives up.

## equalizer

while something plays, the daemon listens to what the laptop plays (the default output's monitor,
through `parec`, 8 kHz mono), splits it into 8 bands every 80 ms and sends `eq 13579753`, bass
first, at most 12 times a second and only when the levels changed. a slow auto gain makes quiet
and loud songs both fill the bars.

privacy: the sound is analysed on the laptop and thrown away right after. it is never recorded,
never saved and never sent. only the 8 level digits go to the pet, nothing goes to the internet.
the capture only runs while the music watcher says something plays, and stops with `music 0`.

- uses numpy for the fft if `/usr/bin/python3` has it, else a simple loudness + brightness guess
- no `parec` (pulseaudio-utils): no equalizer, logged once, the pet makes up its own bars
- turn it off: `TINYCLAWD_NO_EQ=1` in the daemon's environment (for the service:
  `Environment=TINYCLAWD_NO_EQ=1` under `[Service]`)

## hook mapping

| claude code hook | line |
|---|---|
| SessionStart | `ev start` |
| SessionEnd | `ev end` (when no other session is left) |
| UserPromptSubmit | `ev prompt` |
| PreToolUse | `ev tool <tool_name>`, `AskUserQuestion` / `ExitPlanMode` give `ev perm` |
| PreToolUse, Bash | the command picks the look: `Grep` (grep, rg, find), `Read` (cat, ls, sed -n), `Build` (make, idf.py, cargo build), `Test`, `Git`, `Install` (pip, apt, npm install), `WebFetch` (curl, wget), else `Bash` |
| PermissionRequest | `ev perm` |
| Notification `permission_prompt`, `elicitation_dialog`, `agent_needs_input`... | `ev perm` |
| PostToolUse, Bash test command | `ev test pass` (or `fail` if the output says so, e.g. piped into tail) |
| PostToolUse, Bash `git commit` with a `[branch hash]` line | `ev commit` |
| PostToolUse with `interrupted` | `ev interrupt` |
| PostToolUseFailure | `ev toolfail <tool_name>`, plus `ev test fail` for test commands |
| PostToolUseFailure with `is_interrupt` | `ev interrupt` |
| Stop / StopFailure | `ev done` |
| SubagentStart / SubagentStop | `ev sub N` (counted by `agent_id`) |
| PreCompact | `ev compact` |

test commands: pytest, `python -m unittest`, tox, nox, npm/pnpm/yarn/bun test, jest, vitest, mocha,
cargo test, go test, ctest, make test/check, meson test, `python test_x.py`. builds (idf.py build)
are not tests.

checked against claude code 2.1.289 (the strings in the binary):

- every hook gets `session_id`, `transcript_path`, `cwd`, `hook_event_name`, and `agent_id` when
  it fires inside a subagent
- `PostToolUseFailure` exists: `tool_name`, `tool_input`, `error`, `is_interrupt`. a Bash command
  with a non zero exit lands here (error starts with `Exit code N`), not in PostToolUse
- Bash `tool_response` in PostToolUse is `{stdout, stderr, interrupted, backgroundTaskId?, ...}`,
  no exit code
- `PermissionRequest` fires when the dialog shows. the `permission_prompt` Notification only comes
  after a delay, so it is a backup
- `SubagentStart` / `SubagentStop` have `agent_id` and `agent_type`
- `Stop` has no interrupt flag and i found no hook for esc while claude is only thinking, so
  `ev interrupt` only shows when esc stops a running tool
- hooks support `"async": true`

## troubleshooting

- nothing happens: is the daemon running? `python3 tinyclawd.py send ping` says "daemon not
  running" if not. socket is `$XDG_RUNTIME_DIR/tinyclawd.sock` (or `/run/user/<uid>/tinyclawd.sock`,
  else `/tmp/tinyclawd-<uid>.sock`)
- which port: the daemon logs `connected /dev/ttyACM0` to stderr (`journalctl --user -u
  tinyclawd-daemon -f` for the service). wrong one: `--port`
- permission denied on the port: you need to be in the group that owns it (`dialout`, `uucp` on
  arch), then log out and in. the daemon log names the group
- `idf.py monitor` and the daemon can not share the port, stop one
- test without the board: `daemon --stdout` in one terminal, `replay` or a hook in another

## tests

```bash
python3 test_bridge.py    # hook mapping, sessions, music, typing, equalizer, a daemon on a pty
python3 test_setup.py     # settings merge and undo, status line, the service, in a throwaway HOME
```

no board needed, and your real `~/.claude` is never read or written.

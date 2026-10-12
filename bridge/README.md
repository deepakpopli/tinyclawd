# bridge

the laptop side. claude code hooks and the status line call `tinyclawd.py`, a small daemon owns
the usb serial port and sends [protocol](../firmware/PROTOCOL.md) lines to the pet. many claude
code sessions can run at once, they all go through the same daemon.

```
claude code hooks ──┐
status line ────────┼──> unix socket ──> tinyclawd.py daemon ──> /dev/ttyACM0 ──> pet
send / replay ──────┘
```

needs python 3 and pyserial (only the daemon and `replay --to serial` use it):

```bash
pip install --user pyserial
```

## install

```bash
./install.sh
```

it changes nothing, it prints:

1. the `"hooks"` block for `~/.claude/settings.json`. if you already have a hook on the same event
   (i have one on `Stop`), add our entry to that event's list instead of replacing it.
2. one line for the status line script, right after `input=$(cat)`:
   ```bash
   printf '%s' "$input" | /usr/bin/python3 /path/to/bridge/tinyclawd.py usage >/dev/null 2>&1 &
   ```
3. how to run the daemon, by hand or as a systemd user service (`tinyclawd-daemon.service`).

the hooks are `async`, so claude never waits for them. `hook` and `usage` never print anything and
always exit 0, if the daemon is down the event is just dropped. the daemon is not started by the
hooks, run it yourself or use the service.

## commands

```bash
python3 tinyclawd.py daemon                 # finds the pet (espressif usb id, then /dev/ttyACM*)
python3 tinyclawd.py daemon --port /dev/ttyACM1
python3 tinyclawd.py daemon --stdout        # no board, prints what it would send
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
- reconnects when the board is unplugged, and says hello again when the pet prints `boot`
- ignores everything from the pet that is not `ok` / `err` / `touch` / `boot` (esp-idf logs),
  logs touches to stderr
- keeps state per session: subagent counts are summed over all sessions (`ev sub N`), one
  session's `ev done` or `ev end` does not hide another session that is waiting for you (`ev perm`),
  `ev end` only goes out when the last session ends, usage is only forwarded when it changed or
  every 30 s
- asks `playerctl` every 2 s if something plays (spotify, youtube in a browser...), sends
  `music 1 artist - title` when it starts or the track changes and `music 0` after 5 s of quiet.
  no playerctl, no music mode

## hook mapping

| claude code hook | line |
|---|---|
| SessionStart | `ev start` |
| SessionEnd | `ev end` (when no other session is left) |
| UserPromptSubmit | `ev prompt` |
| PreToolUse | `ev tool <tool_name>`, `AskUserQuestion` / `ExitPlanMode` give `ev perm` |
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
  running" if not. socket is `$XDG_RUNTIME_DIR/tinyclawd.sock` (or `/tmp/tinyclawd-<uid>.sock`)
- which port: the daemon logs `connected /dev/ttyACM0` to stderr (`journalctl --user -u
  tinyclawd-daemon -f` for the service). wrong one: `--port`
- permission denied on the port: you need to be in the `dialout` group
- `idf.py monitor` and the daemon can not share the port, stop one
- test without the board: `daemon --stdout` in one terminal, `replay` or a hook in another

## tests

```bash
python3 test_bridge.py
```

no board needed, the daemon test talks to a pty.

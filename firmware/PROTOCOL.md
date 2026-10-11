# protocol (v1)

plain text lines over the USB serial port (115200, but USB-Serial-JTAG ignores baud).
one message per line, `\n` at the end, max 120 chars. the pet answers every line with
`ok ...` or `err ...`. the pet never needs the laptop: with nothing connected it just lives
on its own.

## laptop -> pet

| line | meaning |
|---|---|
| `hello 1` | handshake, pet answers `ok tinyclawd 1` |
| `ping` | same answer as hello |
| `ev start` | a claude code session started |
| `ev end` | session ended |
| `ev prompt` | you sent a prompt, claude is thinking |
| `ev tool <Name>` | a tool started: `Bash`, `Read`, `Edit`, `Write`, `Grep`, `Glob`, `WebFetch`, `WebSearch`, `Task`, `TodoWrite`, `mcp__x__y`... |
| `ev toolfail <Name>` | that tool returned an error |
| `ev perm` | claude is waiting for you (permission or a question) |
| `ev done` | claude finished its turn |
| `ev sub <n>` | n subagents are running now (0 when they are all back) |
| `ev test pass` / `ev test fail` | a test command finished |
| `ev commit` | a git commit went through |
| `ev interrupt` | you pressed esc |
| `ev compact` | the context was compacted |
| `usage 5h=42 wk=18 ctx=37 reset=135` | usage in %, `reset` = minutes until the 5 hour window resets. any field can be left out |
| `time 23:15` | local time, so it knows when it is night |
| `touch tap` / `touch double` / `touch long` | fake a touch (testing) |
| `play <anim>` | play one animation now (testing) |
| `now` | pet answers `ok <anim playing>` |
| `tune fast` / `tune normal` | short idle timings (bored after 30 s, asleep after 3 min) for testing |

unknown lines get `err unknown`, the pet keeps going.

## pet -> laptop

| line | meaning |
|---|---|
| `ok` / `ok <info>` | answer to a line |
| `err <why>` | answer to a bad line |
| `touch tap` / `touch double` / `touch long` | the head was touched |
| `boot tinyclawd 1` | printed once at start |

## example

```
> hello 1
< ok tinyclawd 1
> ev prompt
< ok
> ev tool Bash
< ok
> ev done
< ok
```

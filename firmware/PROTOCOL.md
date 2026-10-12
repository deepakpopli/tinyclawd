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
| `ev perm <project>` | same, and which project is waiting (ascii, up to 16 chars), shown small at the bottom |
| `ev perm +<n>` | n sessions are waiting, shown as `2 waiting` |
| `ev done` | claude finished its turn |
| `ev done small` | one session finished while others still work: a short party, the next line is what the others do (`ev prompt` or `ev tool <Name>`) |
| `ev sub <n>` | n subagents are running now (0 when they are all back) |
| `ev test pass` / `ev test fail` | a test command finished |
| `ev commit` | a git commit went through |
| `ev interrupt` | you pressed esc |
| `ev compact` | the context was compacted |
| `sessions <n>` | n claude code sessions are busy. 2 or more: small dots in the corner while working. only sent when 2+ were involved, so `sessions 1` / `sessions 0` end it |
| `usage 5h=42 wk=18 ctx=37 reset=135` | usage in %, `reset` = minutes until the 5 hour window resets. any field can be left out |
| `music 1 <artist - title>` | music started or the track changed. title is ascii, up to 48 chars, may be empty |
| `music 0` | music stopped |
| `typing <n>` | you are typing on the laptop, n = keys per second (1-15). `typing 0` = stopped. sent when it changes, and again every 5 s while it goes on. only a count, never which keys |
| `time 23:15` | local time, so it knows when it is night |
| `touch tap` / `touch double` / `touch long` | fake a touch (testing) |
| `touch down` / `touch up` | fake the pad edges, hold to open the menu (testing) |
| `play <anim>` | play one animation now (testing) |
| `now` | pet answers `ok <anim playing>`, or where the menu is: `ok menu usage`, `ok games keepy`, `ok game run` |
| `tune fast` / `tune normal` | short idle timings (bored after 30 s, asleep after 3 min) for testing |

unknown lines get `err unknown`, the pet keeps going.

### hw lines (bring up and tuning)

| line | meaning |
|---|---|
| `hw` | answers `ok offset=2 flip=0 mirror=0 contrast=207 i2c=400 double=280 long=700 test=0 screen=ok` |
| `hw offset <0-4>` | first ram column of the panel |
| `hw flip <0/1>` | turn the picture 180 degrees |
| `hw mirror <0/1>` | swap left and right |
| `hw contrast <0-255>` | brightness |
| `hw i2c <100-1000>` | i2c clock in kHz |
| `hw double <100-1000>` / `hw long <300-3000>` | touch timings in ms |
| `hw test <0-3>` | 0 pet, 1 border + corners + arrow, 2 all on, 3 checker (worst case flush) |
| `hw touchraw <0/1>` | print `raw 1` / `raw 0` on every touch pin change |
| `hw stats` | `ok fps=40.0 draw=310/900 flush=4100/22800 us avg/max pages=1.6`, since the last `hw stats` |
| `hw scan` | `ok found 1 0x3C`, i2c addresses that answer |
| `hw save` / `hw defaults` | keep the settings in flash / go back to the built-in ones (not saved) |

the setting lines answer with the full `hw` line, so you see what changed. a bad value gets `err want 0..4`.

## pet -> laptop

| line | meaning |
|---|---|
| `ok` / `ok <info>` | answer to a line |
| `err <why>` | answer to a bad line |
| `touch tap` / `touch double` / `touch long` | the head was touched (down / up edges are not printed) |
| `boot tinyclawd 1` | printed once at start |
| `raw 1` / `raw 0` | touch pin changed (only after `hw touchraw 1`) |

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

two sessions, one finishes while the other is still running a command:

```
> ev done small
> ev tool Bash
> sessions 1
```

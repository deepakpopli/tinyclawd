#!/usr/bin/env python3
# tiny clawd bridge: claude code hooks + status line -> usb serial -> pet
#
#   tinyclawd.py daemon [--port P] [--stdout]   owns the serial port
#   tinyclawd.py hook                           called by claude code hooks (json on stdin)
#   tinyclawd.py usage                          called from the status line (json on stdin)
#   tinyclawd.py send "ev perm"                 one raw line
#   tinyclawd.py replay [file] [--speed 2]      play a fake session
#
# only needs pyserial, and only for the daemon / replay --to serial.

import argparse
import glob
import json
import os
import re
import select
import signal
import socket
import subprocess
import sys
import threading
import time

PROTO = 1
MAX_LINE = 120
ESPRESSIF_VID = 0x303A
HERE = os.path.dirname(os.path.abspath(__file__))
DEMO_FILE = os.path.join(HERE, "..", "firmware", "sim", "scripts", "demo.txt")


def sock_path():
    if os.environ.get("TINYCLAWD_SOCK"):
        return os.environ["TINYCLAWD_SOCK"]
    run = os.environ.get("XDG_RUNTIME_DIR")
    if run and os.path.isdir(run):
        return os.path.join(run, "tinyclawd.sock")
    return "/tmp/tinyclawd-%d.sock" % os.getuid()


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, file=sys.stderr, flush=True)


def clean(s, n=60):
    # tool names etc go on one short ascii line
    s = re.sub(r"[^\x21-\x7e]", "", str(s))
    return s[:n] or "?"


# ---------------------------------------------------------------- socket client

def send_to_daemon(lines, timeout=0.2, path=None):
    """send lines (str or list) to the daemon. returns False if it is not there."""
    if isinstance(lines, str):
        lines = [lines]
    data = "".join(l.rstrip("\n") + "\n" for l in lines).encode()
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        s.settimeout(timeout)
        s.connect(path or sock_path())
        s.sendall(data)
        return True
    except OSError:
        return False
    finally:
        s.close()


def daemon_running(path=None):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        s.settimeout(0.2)
        s.connect(path or sock_path())
        return True
    except OSError:
        return False
    finally:
        s.close()


# ---------------------------------------------------------------- hook mapping

TEST_RE = re.compile(
    r"(^|[\s;&|(/])("
    r"pytest|py\.test|python3? -m (pytest|unittest)|tox|nox|"
    r"(npm|pnpm|yarn|bun)( run)? test|jest|vitest|mocha|"
    r"cargo (test|nextest)|go test|ctest|make (test|check)|meson test|"
    r"python3? \S*test\S*\.py"
    r")\b"
)
GIT_COMMIT_RE = re.compile(r"\bgit(\s+-[cC]\s+\S+)*\s+commit\b")
COMMIT_OUT_RE = re.compile(r"^\[[^\]\n]+ [0-9a-f]{7,}\]", re.M)
FAIL_OUT_RE = re.compile(
    r"(\b\d+ (failed|failing|errors?)\b|^FAILED\b|^FAIL\b|test result: FAILED|^ERRORS?\b)", re.M
)
PERM_NOTES = {
    "permission_prompt", "elicitation_dialog", "elicitation_url_dialog",
    "worker_permission_prompt", "agent_needs_input",
}
ASKING_TOOLS = {"AskUserQuestion", "ExitPlanMode"}


INSTALL_RE = re.compile(r"\b(pip3?|uv pip|uv|apt(-get)?|brew|conda|npm|pnpm|yarn|bun) (install|add|i)\b[^;&|]*")


def is_test(cmd):
    # "pip install pytest" is not a test run
    return bool(TEST_RE.search(INSTALL_RE.sub("", cmd or "")))


def map_hook(d):
    """hook json -> list of protocol lines (can be empty)"""
    ev = d.get("hook_event_name", "")
    tool = d.get("tool_name") or ""
    tin = d.get("tool_input") if isinstance(d.get("tool_input"), dict) else {}
    cmd = tin.get("command", "") if tool == "Bash" else ""

    if ev == "SessionStart":
        return ["ev start"]
    if ev == "SessionEnd":
        return ["ev end"]
    if ev == "UserPromptSubmit":
        return ["ev prompt"]
    if ev == "PreToolUse":
        if tool in ASKING_TOOLS:
            return ["ev perm"]
        return ["ev tool " + clean(tool)]
    if ev == "PermissionRequest":
        return ["ev perm"]
    if ev == "Notification":
        if d.get("notification_type") in PERM_NOTES:
            return ["ev perm"]
        return []
    if ev in ("Stop", "StopFailure"):
        return ["ev done"]
    if ev == "PreCompact":
        return ["ev compact"]

    if ev == "PostToolUse":
        r = d.get("tool_response")
        r = r if isinstance(r, dict) else {}
        if r.get("interrupted"):
            return ["ev interrupt"]
        if tool != "Bash" or r.get("backgroundTaskId"):
            return []
        out = "%s\n%s" % (r.get("stdout") or "", r.get("stderr") or "")
        if is_test(cmd):
            return ["ev test fail" if FAIL_OUT_RE.search(out) else "ev test pass"]
        if GIT_COMMIT_RE.search(cmd) and "--dry-run" not in cmd:
            if COMMIT_OUT_RE.search(out) or " -q" in cmd:
                return ["ev commit"]
        return []

    if ev == "PostToolUseFailure":
        if d.get("is_interrupt"):
            return ["ev interrupt"]
        lines = ["ev toolfail " + clean(tool)]
        if tool == "Bash" and is_test(cmd):
            lines.append("ev test fail")
        return lines

    # SubagentStart / SubagentStop are counted by the daemon
    return []


def hook_message(d):
    """what the hook sends to the daemon: one json line, the daemon keeps per session state"""
    return json.dumps({
        "sid": str(d.get("session_id") or "?"),
        "ts": time.time(),
        "event": d.get("hook_event_name", ""),
        "aid": d.get("agent_id") or "",
        "lines": map_hook(d),
    })


def cmd_hook(args):
    # stdout of a hook can end up in the session, so never print anything
    try:
        sys.stdout = open(os.devnull, "w")
        raw = sys.stdin.read()
        d = json.loads(raw) if raw.strip() else {}
        if isinstance(d, dict) and d.get("hook_event_name"):
            send_to_daemon(hook_message(d), timeout=0.2, path=args.sock)
    except BaseException:
        pass
    os._exit(0)


# ---------------------------------------------------------------- usage

def dig(d, *keys):
    for k in keys:
        if not isinstance(d, dict):
            return None
        d = d.get(k)
    return d


def as_num(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def reset_minutes(v, now=None):
    """resets_at is unix seconds (the status line does date -d @x). also take ms or iso text."""
    now = time.time() if now is None else now
    t = as_num(v)
    if t is None and isinstance(v, str) and v:
        try:
            from datetime import datetime
            t = datetime.fromisoformat(v.replace("Z", "+00:00")).timestamp()
        except ValueError:
            return None
    if t is None:
        return None
    if t > 1e12:
        t /= 1000.0
    return max(0, int(round((t - now) / 60.0)))


def usage_line(d, now=None):
    parts = []
    for name, keys in (
        ("5h", ("rate_limits", "five_hour", "used_percentage")),
        ("wk", ("rate_limits", "seven_day", "used_percentage")),
        ("ctx", ("context_window", "used_percentage")),
    ):
        v = as_num(dig(d, *keys))
        if v is not None:
            parts.append("%s=%d" % (name, max(0, min(100, round(v)))))
    m = reset_minutes(dig(d, "rate_limits", "five_hour", "resets_at"), now)
    if m is not None:
        parts.append("reset=%d" % m)
    return "usage " + " ".join(parts) if parts else None


def cmd_usage(args):
    try:
        sys.stdout = open(os.devnull, "w")
        d = json.loads(sys.stdin.read() or "{}")
        line = usage_line(d) if isinstance(d, dict) else None
        if line:
            send_to_daemon(line, timeout=0.2, path=args.sock)
    except BaseException:
        pass
    os._exit(0)


# ---------------------------------------------------------------- serial link

def find_port():
    try:
        from serial.tools import list_ports
        for p in list_ports.comports():
            if p.vid == ESPRESSIF_VID:
                return p.device
    except Exception:
        pass
    ports = sorted(glob.glob("/dev/ttyACM*"))
    return ports[0] if ports else None


class Link:
    """the serial port, reconnecting. with stdout=True it just prints lines."""

    def __init__(self, port=None, stdout=False, on_connect=None, on_line=None):
        self.port = port
        self.stdout = stdout
        self.on_connect = on_connect or (lambda: None)
        self.on_line = on_line or (lambda l: None)
        self.ser = None
        self.lock = threading.Lock()
        self.stop = False

    def connected(self):
        return self.stdout or self.ser is not None

    def send(self, line):
        line = line.strip()[:MAX_LINE]
        if not line:
            return False
        if self.stdout:
            print(line, flush=True)
            return True
        with self.lock:
            if self.ser is None:
                return False
            try:
                self.ser.write((line + "\n").encode("ascii", "replace"))
                return True
            except Exception as e:
                log("write failed:", e)
                self._drop()
                return False

    def _drop(self):
        try:
            self.ser.close()
        except Exception:
            pass
        self.ser = None

    def _open(self):
        import serial
        port = self.port or find_port()
        if not port:
            return False
        try:
            s = serial.Serial(port, 115200, timeout=0.5)
        except Exception:
            return False
        with self.lock:
            self.ser = s
        log("connected", port)
        return True

    def run(self):
        if self.stdout:
            self.on_connect()
            while not self.stop:
                time.sleep(0.5)
            return
        buf = b""
        while not self.stop:
            if self.ser is None:
                if not self._open():
                    time.sleep(2)
                    continue
                buf = b""
                self.on_connect()
            try:
                chunk = self.ser.read(256)
            except Exception as e:
                log("port gone:", e)
                with self.lock:
                    self._drop()
                continue
            buf += chunk
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                self.on_line(raw.decode("ascii", "replace").strip())
            if len(buf) > 4096:
                buf = b""


# ---------------------------------------------------------------- daemon state

class Session:
    def __init__(self):
        self.subs = set()
        self.gone = set()
        self.perm = 0.0
        self.done_ts = 0.0
        self.seen = time.time()


class State:
    """per session bookkeeping so many claude code sessions can share one pet"""

    PERM_HOLD = 300      # a pending perm hides other sessions' tool noise this long
    SESSION_TTL = 3600   # forget a silent session (crashed, no SessionEnd)
    USAGE_EVERY = 30

    def __init__(self, out):
        self.out = out   # function(line)
        self.sessions = {}
        self.last_sub = 0
        self.last_usage = None
        self.usage_sent = 0.0

    def subs(self):
        return sum(len(s.subs) for s in self.sessions.values())

    def perm_waiting(self, other_than=None, hold=False, now=None):
        now = now or time.time()
        for sid, s in self.sessions.items():
            if sid != other_than and s.perm and (not hold or now - s.perm < self.PERM_HOLD):
                return True
        return False

    def send_subs(self):
        n = self.subs()
        if n != self.last_sub:
            self.last_sub = n
            self.out("ev sub %d" % n)

    def prune(self, now):
        for sid in [k for k, s in self.sessions.items() if now - s.seen > self.SESSION_TTL]:
            del self.sessions[sid]
        self.send_subs()

    def raw(self, line, now=None):
        """a plain protocol line from send / usage / replay"""
        now = now or time.time()
        if line.startswith("usage "):
            if line == self.last_usage and now - self.usage_sent < self.USAGE_EVERY:
                return
            self.last_usage = line
            self.usage_sent = now
        self.out(line)

    def hook(self, m, now=None):
        now = now or time.time()
        sid = str(m.get("sid") or "?")
        ev = m.get("event") or ""
        lines = m.get("lines")
        lines = [l for l in lines if isinstance(l, str)] if isinstance(lines, list) else []
        ts = as_num(m.get("ts")) or now
        aid = str(m.get("aid") or "")

        if ev == "SessionEnd":
            self.sessions.pop(sid, None)
            if not self.sessions:
                self.last_sub = 0
                for l in lines:
                    self.out(l)
            else:
                self.send_subs()
                if self.perm_waiting():
                    self.out("ev perm")
            return

        s = self.sessions.get(sid)
        if s is None:
            s = self.sessions[sid] = Session()
        s.seen = now

        if ev == "SubagentStart" and aid and aid not in s.gone:
            s.subs.add(aid)
            self.send_subs()
            return
        if ev == "SubagentStop" and aid:
            s.subs.discard(aid)
            s.gone.add(aid)
            if len(s.gone) > 200:
                s.gone.clear()
            self.send_subs()
            return

        if ev in ("PermissionRequest", "Notification") and lines:
            s.perm = now
        elif ev in ("PostToolUse", "PostToolUseFailure", "UserPromptSubmit",
                    "Stop", "StopFailure", "PermissionDenied"):
            s.perm = 0.0
        if ev == "PreToolUse" and lines == ["ev perm"]:
            s.perm = now

        if ev in ("Stop", "StopFailure"):
            s.done_ts = ts
            if self.perm_waiting():
                self.out("ev perm")
                return
        if ev == "UserPromptSubmit":
            s.done_ts = 0.0

        if ev == "PreToolUse":
            if ts < s.done_ts:
                return   # late async hook from before the turn ended
            if lines != ["ev perm"] and self.perm_waiting(hold=True, now=now):
                return   # someone is waiting for you, keep showing that
        for l in lines:
            self.out(l)
        # a test or commit from another session still should not hide a waiting perm
        if lines and ev in ("PostToolUse", "PostToolUseFailure", "PreCompact") \
                and self.perm_waiting(other_than=sid):
            self.out("ev perm")


# ---------------------------------------------------------------- music

PLAYER_FORMAT = "{{status}}\t{{playerName}}\t{{artist}}\t{{title}}"


def playerctl():
    """one line per player, '' when nothing plays, None when playerctl is not installed"""
    try:
        p = subprocess.run(["playerctl", "-a", "metadata", "--format", PLAYER_FORMAT],
                           capture_output=True, text=True, timeout=2)
    except FileNotFoundError:
        return None
    except (OSError, subprocess.TimeoutExpired):
        return ""
    # exit 1 + "No players found" on stderr when nothing is open. with many players one
    # can fail and still leave good lines, so look at stdout either way
    if p.returncode != 0 and not p.stdout.strip():
        return ""
    return p.stdout


def music_line(text):
    """playerctl output -> 'music 1 artist - title' for the first playing player, else None"""
    for row in (text or "").splitlines():
        f = row.split("\t", 3)
        if len(f) < 4 or f[0] != "Playing":
            continue
        artist, title = f[2].strip(), f[3].strip()
        name = "%s - %s" % (artist, title) if artist and title else artist or title
        name = re.sub(r"[^\x20-\x7e]", "", name)
        name = re.sub(r" +", " ", name).strip()[:48].strip()
        return ("music 1 " + name).strip()
    return None


class Music:
    """polls playerctl, sends music 1 on start / track change, music 0 after 5 s of silence"""

    EVERY = 2
    OFF_AFTER = 5   # pauses and skips are shorter than this, no flicker

    def __init__(self, out, run=playerctl):
        self.out = out   # function(line)
        self.run = run   # function() -> playerctl output
        self.line = None   # what the pet shows now, None = no music
        self.last_play = 0.0
        self.off = False
        self.stop = False

    def poll(self, now=None):
        if self.off:
            return
        now = now or time.time()
        text = self.run()
        if text is None:
            log("playerctl not found, no music mode")
            self.off = True
            return
        line = music_line(text)
        if line:
            self.last_play = now
            if line != self.line:
                self.line = line
                self.out(line)
        elif self.line and now - self.last_play >= self.OFF_AFTER:
            self.line = None
            self.out("music 0")

    def loop(self):
        while not self.stop and not self.off:
            try:
                self.poll()
            except Exception as e:
                log("music:", e)
            time.sleep(self.EVERY)


# ---------------------------------------------------------------- daemon

DEVICE_PREFIXES = ("ok", "err", "touch", "boot")


class Daemon:
    def __init__(self, port=None, stdout=False, path=None, player=playerctl):
        self.path = path or sock_path()
        self.link = Link(port, stdout, self.on_connect, self.on_device)
        self.state = State(self.link.send)
        self.music = Music(self.link.send, player)
        self.last_time = 0.0

    def on_connect(self):
        self.link.send("hello %d" % PROTO)
        self.send_time()
        if self.state.last_usage:
            self.link.send(self.state.last_usage)
        if self.state.subs():
            self.link.send("ev sub %d" % self.state.subs())
        if self.state.perm_waiting():
            self.link.send("ev perm")
        if self.music.line:
            self.link.send(self.music.line)

    def send_time(self):
        self.last_time = time.time()
        self.link.send("time " + time.strftime("%H:%M"))

    def on_device(self, line):
        word = line.split(" ", 1)[0]
        if word not in DEVICE_PREFIXES:
            return   # esp-idf logs share the port
        if word == "touch":
            log("pet:", line)
        elif word == "boot":
            log("pet rebooted:", line)
            self.on_connect()
        elif word == "err" or line.startswith("ok tinyclawd"):
            log("pet:", line)

    def handle_client(self, conn):
        conn.settimeout(0.5)
        data = b""
        try:
            while len(data) < 65536:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
        except OSError:
            pass
        finally:
            conn.close()
        for raw in data.decode("utf-8", "replace").splitlines():
            raw = raw.strip()
            if not raw:
                continue
            try:
                if raw.startswith("{"):
                    m = json.loads(raw)
                    if isinstance(m, dict):
                        self.state.hook(m)
                else:
                    self.state.raw(raw)
            except Exception as e:
                log("bad message:", e)

    def listen(self):
        if daemon_running(self.path):
            log("already running on", self.path)
            return None
        try:
            os.unlink(self.path)
        except OSError:
            pass
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        old = os.umask(0o177)
        try:
            srv.bind(self.path)
        finally:
            os.umask(old)
        srv.listen(16)
        return srv

    def run(self, until=None):
        srv = self.listen()
        if srv is None:
            return 1
        log("listening on", self.path)
        t = threading.Thread(target=self.link.run, daemon=True)
        t.start()
        threading.Thread(target=self.music.loop, daemon=True).start()
        try:
            while until is None or not until.is_set():
                r, _, _ = select.select([srv], [], [], 1.0)
                if r:
                    try:
                        conn, _ = srv.accept()
                        self.handle_client(conn)
                    except OSError:
                        pass
                now = time.time()
                if self.link.connected() and now - self.last_time > 300:
                    self.send_time()
                self.state.prune(now)
        except KeyboardInterrupt:
            pass
        finally:
            self.link.stop = True
            self.music.stop = True
            srv.close()
            try:
                os.unlink(self.path)
            except OSError:
                pass
        return 0


def on_term(*a):
    raise KeyboardInterrupt


def cmd_daemon(args):
    if not args.stdout:
        try:
            import serial  # noqa: F401
        except ImportError:
            log("pyserial missing: pip install --user pyserial")
            return 1
    # systemd stops us with SIGTERM, clean up the socket like on ctrl-c
    signal.signal(signal.SIGTERM, on_term)
    return Daemon(args.port, args.stdout, args.sock).run()


# ---------------------------------------------------------------- replay

FALLBACK_SESSION = """
# built in fake session, used when firmware/sim/scripts/demo.txt is missing
0 hello 1
+200 usage 5h=42 wk=18 ctx=12 reset=135
+500 ev start
+1500 ev prompt
+3000 ev tool Read
+1500 ev tool Grep
+1500 ev tool Edit
+2000 ev tool Bash
+2500 ev test fail
+1500 ev tool Edit
+1500 ev tool Bash
+2500 ev test pass
+1000 ev tool Task
+500 ev sub 2
+4000 ev sub 1
+2500 ev sub 0
+1000 ev tool Bash
+1500 ev perm
+5000 ev tool Bash
+2000 ev commit
+1500 usage 5h=47 wk=19 ctx=31 reset=128
+500 ev done
+6000 ev prompt
+2000 ev compact
+3000 ev tool WebSearch
+2000 ev toolfail WebFetch
+1500 ev interrupt
+3000 ev end
"""


def parse_script(text):
    """'<ms> line' absolute or '+<ms> line' relative -> [(ms, line)]"""
    out = []
    t = 0
    for n, raw in enumerate(text.splitlines(), 1):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        parts = s.split(None, 1)
        if len(parts) < 2:
            raise ValueError("line %d: need '<ms> <line>'" % n)
        stamp, line = parts
        try:
            if stamp.startswith("+"):
                t += int(stamp[1:])
            else:
                t = int(stamp)
        except ValueError:
            raise ValueError("line %d: bad time %r" % (n, stamp))
        out.append((t, line.strip()))
    return out


def cmd_replay(args):
    path = args.file or DEMO_FILE
    if os.path.exists(path):
        text = open(path).read()
        src = path
    elif args.file:
        log("no such file:", path)
        return 1
    else:
        text = FALLBACK_SESSION
        src = "built in session"
    try:
        script = parse_script(text)
    except ValueError as e:
        log(e)
        return 1

    to = args.to or ("daemon" if daemon_running(args.sock) else "stdout")
    link = None
    if to == "serial":
        if daemon_running(args.sock):
            log("the daemon owns the port, use --to daemon (or stop it)")
            return 1
        link = Link(args.port, on_line=lambda l: l.split(" ", 1)[0] in DEVICE_PREFIXES and log("pet:", l))
        try:
            if not link._open():
                log("no pet found, try --port")
                return 1
        except ImportError:
            log("pyserial missing: pip install --user pyserial")
            return 1
        threading.Thread(target=link.run, daemon=True).start()
        link.send("hello %d" % PROTO)
        time.sleep(0.3)

    def out(line):
        if to == "daemon":
            if not send_to_daemon(line, path=args.sock):
                log("daemon not answering")
        elif to == "serial":
            link.send(line)
        else:
            print(line, flush=True)

    speed = args.speed if args.speed > 0 else 1.0
    log("replaying %s (%d lines) to %s" % (src, len(script), to))
    try:
        while True:
            start = time.time()
            for ms, line in script:
                wait = start + ms / 1000.0 / speed - time.time()
                if wait > 0:
                    time.sleep(wait)
                out(line)
            if not args.loop:
                break
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    if link:
        link.stop = True
    return 0


def cmd_console(args):
    """type protocol lines by hand and see every line the pet prints (bring up, tuning)"""
    if daemon_running(args.sock):
        log("the daemon owns the port, stop it first")
        return 1
    try:
        import serial  # noqa: F401
    except ImportError:
        log("pyserial missing: pip install --user pyserial")
        return 1
    link = Link(args.port, on_line=lambda l: l and print("<", l, flush=True))
    threading.Thread(target=link.run, daemon=True).start()
    print("type a line and press enter, ctrl-d to quit. try: hw, hw test 1, hw stats")
    try:
        for line in sys.stdin:
            if line.strip() and not link.send(line):
                log("not connected yet")
    except KeyboardInterrupt:
        pass
    link.stop = True
    return 0


def cmd_send(args):
    line = " ".join(args.line)
    if not send_to_daemon(line, path=args.sock):
        log("daemon not running (%s)" % (args.sock or sock_path()))
        return 1
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(prog="tinyclawd", description="tiny clawd laptop bridge")
    p.add_argument("--sock", help="unix socket (default %s)" % sock_path())
    sub = p.add_subparsers(dest="cmd", required=True)

    d = sub.add_parser("daemon", help="own the serial port and forward events")
    d.add_argument("--port", help="serial port, default: auto detect")
    d.add_argument("--stdout", "--dry-run", action="store_true", help="print lines, no serial")
    d.set_defaults(fn=cmd_daemon)

    sub.add_parser("hook", help="claude code hook, json on stdin").set_defaults(fn=cmd_hook)
    sub.add_parser("usage", help="status line json on stdin").set_defaults(fn=cmd_usage)

    s = sub.add_parser("send", help="send one raw protocol line")
    s.add_argument("line", nargs="+")
    s.set_defaults(fn=cmd_send)

    c = sub.add_parser("console", help="type lines to the pet, print its answers")
    c.add_argument("--port")
    c.set_defaults(fn=cmd_console)

    r = sub.add_parser("replay", help="play a timed session file")
    r.add_argument("file", nargs="?")
    r.add_argument("--speed", type=float, default=1.0)
    r.add_argument("--to", choices=["serial", "daemon", "stdout"])
    r.add_argument("--port")
    r.add_argument("--loop", action="store_true")
    r.set_defaults(fn=cmd_replay)

    args = p.parse_args(argv)
    return args.fn(args) or 0


if __name__ == "__main__":
    sys.exit(main())

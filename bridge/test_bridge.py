#!/usr/bin/env python3
# no hardware needed: python3 bridge/test_bridge.py

import json
import os
import select
import subprocess
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tinyclawd as tc  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))


def hook(event, **kw):
    d = {"session_id": "s1", "transcript_path": "/tmp/x", "cwd": "/tmp", "hook_event_name": event}
    d.update(kw)
    return d


def bash(cmd, stdout="", stderr="", interrupted=False):
    return hook("PostToolUse", tool_name="Bash", tool_input={"command": cmd},
                tool_response={"stdout": stdout, "stderr": stderr, "interrupted": interrupted})


class HookMapping(unittest.TestCase):
    def test_simple_events(self):
        self.assertEqual(tc.map_hook(hook("SessionStart", source="startup")), ["ev start"])
        self.assertEqual(tc.map_hook(hook("SessionEnd", reason="exit")), ["ev end"])
        self.assertEqual(tc.map_hook(hook("UserPromptSubmit", prompt="hi")), ["ev prompt"])
        self.assertEqual(tc.map_hook(hook("Stop", stop_hook_active=False)), ["ev done"])
        self.assertEqual(tc.map_hook(hook("PreCompact", trigger="auto")), ["ev compact"])
        self.assertEqual(tc.map_hook(hook("SubagentStart", agent_id="a", agent_type="x")), [])
        self.assertEqual(tc.map_hook(hook("SomethingNew")), [])
        self.assertEqual(tc.map_hook({}), [])

    def test_tools(self):
        self.assertEqual(tc.map_hook(hook("PreToolUse", tool_name="Bash", tool_input={})),
                         ["ev tool Bash"])
        self.assertEqual(tc.map_hook(hook("PreToolUse", tool_name="Agent")), ["ev tool Agent"])
        self.assertEqual(tc.map_hook(hook("PreToolUse", tool_name="mcp__x__y z\n")),
                         ["ev tool mcp__x__yz"])
        self.assertEqual(tc.map_hook(hook("PreToolUse", tool_name="AskUserQuestion")), ["ev perm"])
        self.assertEqual(tc.map_hook(hook("PostToolUse", tool_name="Read", tool_response={})), [])

    def test_perm(self):
        self.assertEqual(tc.map_hook(hook("PermissionRequest", tool_name="Bash")), ["ev perm"])
        self.assertEqual(tc.map_hook(hook("Notification", notification_type="permission_prompt",
                                          message="Claude needs your permission")), ["ev perm"])
        self.assertEqual(tc.map_hook(hook("Notification", notification_type="idle_prompt")), [])

    def test_tests_and_commits(self):
        self.assertEqual(tc.map_hook(bash("pytest -q", "3 passed")), ["ev test pass"])
        self.assertEqual(tc.map_hook(bash("cd x && npm test")), ["ev test pass"])
        self.assertEqual(tc.map_hook(bash("pytest | tail -3", "1 failed, 2 passed")), ["ev test fail"])
        self.assertEqual(tc.map_hook(bash("cargo test", "test result: FAILED. 1 passed")),
                         ["ev test fail"])
        self.assertEqual(tc.map_hook(bash("python3 bridge/test_bridge.py", "", "OK")), ["ev test pass"])
        self.assertEqual(tc.map_hook(bash("ls -la")), [])
        self.assertEqual(tc.map_hook(bash("pip install pytest")), [])
        self.assertEqual(tc.map_hook(bash("npm install && npm test")), ["ev test pass"])
        self.assertEqual(tc.map_hook(bash("git commit -m 'x'", "[main 1a2b3c4] x\n 1 file changed")),
                         ["ev commit"])
        self.assertEqual(tc.map_hook(bash("git -C repo commit -m x", "[main abcdef1] x")), ["ev commit"])
        self.assertEqual(tc.map_hook(bash("git log --grep commit")), [])
        self.assertEqual(tc.map_hook(bash("git commit --dry-run", "[main 1a2b3c4] x")), [])
        self.assertEqual(tc.map_hook(bash("sleep 100", interrupted=True)), ["ev interrupt"])

    def test_failures(self):
        f = hook("PostToolUseFailure", tool_name="Bash", tool_input={"command": "go test ./..."},
                 error="Exit code 1")
        self.assertEqual(tc.map_hook(f), ["ev toolfail Bash", "ev test fail"])
        f = hook("PostToolUseFailure", tool_name="WebFetch", tool_input={}, error="404")
        self.assertEqual(tc.map_hook(f), ["ev toolfail WebFetch"])
        f = hook("PostToolUseFailure", tool_name="Bash", tool_input={}, error="x", is_interrupt=True)
        self.assertEqual(tc.map_hook(f), ["ev interrupt"])

    def test_weird_input(self):
        self.assertEqual(tc.map_hook(hook("PostToolUse", tool_name="Bash", tool_input="nope",
                                          tool_response=None)), [])
        m = json.loads(tc.hook_message(hook("PreToolUse", tool_name="Read", agent_id="ag1")))
        self.assertEqual((m["sid"], m["aid"], m["lines"]), ("s1", "ag1", ["ev tool Read"]))

    def test_screen_slash_commands(self):
        self.assertEqual(tc.map_hook(hook("UserPromptSubmit", prompt="/usage")), ["play usage"])
        self.assertEqual(tc.map_hook(hook("UserPromptSubmit", prompt=" /cost ")), ["play usage"])
        self.assertEqual(tc.map_hook(hook("UserPromptExpansion", command_name="context")), ["play context"])
        self.assertEqual(tc.map_hook(hook("UserPromptExpansion", command_name="review")), [])
        self.assertEqual(tc.map_hook(hook("UserPromptSubmit", prompt="/review the pr")), ["ev prompt"])
        # a screen is not a claude turn, the session must not look busy
        out = []
        st = tc.State(out.append)
        st.hook(json.loads(tc.hook_message(hook("UserPromptSubmit", prompt="/usage"))))
        self.assertEqual(out, ["play usage"])
        self.assertFalse(any(x.busy for x in st.sessions.values()))

    def test_hook_cli_is_silent(self):
        env = dict(os.environ, TINYCLAWD_SOCK="/tmp/tinyclawd-test-nothing-here.sock")
        for data in (json.dumps(hook("Stop")), "not json", ""):
            t = time.time()
            p = subprocess.run([sys.executable, os.path.join(HERE, "tinyclawd.py"), "hook"],
                               input=data.encode(), capture_output=True, env=env, timeout=5)
            self.assertEqual(p.returncode, 0)
            self.assertEqual(p.stdout, b"")
            self.assertLess(time.time() - t, 2)


class Usage(unittest.TestCase):
    def test_full(self):
        d = {"rate_limits": {"five_hour": {"used_percentage": 41.6, "resets_at": 1000 + 135 * 60},
                             "seven_day": {"used_percentage": 18, "resets_at": 99999}},
             "context_window": {"used_percentage": 37.2}, "cost": {"total_cost_usd": 1.2}}
        self.assertEqual(tc.usage_line(d, now=1000), "usage 5h=42 wk=18 ctx=37 reset=135")

    def test_partial(self):
        self.assertEqual(tc.usage_line({"context_window": {"used_percentage": 5}}), "usage ctx=5")
        self.assertIsNone(tc.usage_line({}))
        self.assertIsNone(tc.usage_line({"rate_limits": None}))

    def test_reset_formats(self):
        self.assertEqual(tc.reset_minutes(1600, now=1000), 10)
        self.assertEqual(tc.reset_minutes("1600.5", now=1000), 10)
        self.assertEqual(tc.reset_minutes(1760000600000, now=1760000000), 10)   # ms
        self.assertEqual(tc.reset_minutes("1970-01-01T00:26:40Z", now=1000), 10)
        self.assertEqual(tc.reset_minutes(10, now=1000), 0)
        self.assertIsNone(tc.reset_minutes(None))
        self.assertIsNone(tc.reset_minutes("soon"))


class Replay(unittest.TestCase):
    def test_parse(self):
        s = tc.parse_script("# hi\n\n0 hello 1\n+500 ev prompt\n2000 ev tool Bash\n+250 ev done\n")
        self.assertEqual(s, [(0, "hello 1"), (500, "ev prompt"), (2000, "ev tool Bash"),
                             (2250, "ev done")])

    def test_bad(self):
        with self.assertRaises(ValueError):
            tc.parse_script("abc ev done")
        with self.assertRaises(ValueError):
            tc.parse_script("100")

    def test_fallback_and_demo_parse(self):
        self.assertGreater(len(tc.parse_script(tc.FALLBACK_SESSION)), 10)
        if os.path.exists(tc.DEMO_FILE):
            with open(tc.DEMO_FILE) as f:
                tc.parse_script(f.read())


class SessionState(unittest.TestCase):
    def setUp(self):
        self.out = []   # without the sessions count, these tests are about one thing each
        self.all = []
        self.st = tc.State(self.keep)

    def keep(self, line):
        self.all.append(line)
        if not line.startswith("sessions "):
            self.out.append(line)

    def ev(self, sid, event, lines=(), aid="", ts=None, proj="", tool=""):
        self.st.hook({"sid": sid, "event": event, "lines": list(lines), "aid": aid,
                      "ts": ts or time.time(), "proj": proj, "tool": tool})

    def test_subagents_summed(self):
        self.ev("a", "SubagentStart", aid="1")
        self.ev("b", "SubagentStart", aid="2")
        self.ev("a", "SubagentStart", aid="3")
        self.ev("a", "SubagentStop", aid="1")
        self.ev("a", "SubagentStop", aid="1")
        self.assertEqual(self.out, ["ev sub 1", "ev sub 2", "ev sub 3", "ev sub 2"])

    def test_late_start_after_stop_ignored(self):
        self.ev("a", "SubagentStop", aid="1")
        self.ev("a", "SubagentStart", aid="1")
        self.assertEqual(self.out, [])

    def test_stop_keeps_other_perm(self):
        self.ev("a", "PermissionRequest", ["ev perm"])
        self.ev("b", "PreToolUse", ["ev tool Bash"])
        self.ev("b", "Stop", ["ev done"])
        self.assertEqual(self.out, ["ev perm", "ev perm"])
        self.ev("a", "PostToolUse", [])
        self.ev("b", "UserPromptSubmit", ["ev prompt"])
        self.assertEqual(self.out[-1], "ev prompt")

    def test_end_with_others_alive(self):
        self.ev("a", "SessionStart", ["ev start"])
        self.ev("b", "SessionStart", ["ev start"])
        self.ev("b", "SubagentStart", aid="x")
        self.ev("b", "SessionEnd", ["ev end"])
        self.assertEqual(self.out, ["ev start", "ev start", "ev sub 1", "ev sub 0"])
        self.ev("a", "SessionEnd", ["ev end"])
        self.assertEqual(self.out[-1], "ev end")

    def test_late_tool_after_done(self):
        t = time.time()
        self.ev("a", "Stop", ["ev done"], ts=t)
        self.ev("a", "PreToolUse", ["ev tool Read"], ts=t - 0.05)
        self.assertEqual(self.out, ["ev done"])

    def test_usage_dedup(self):
        self.st.raw("usage 5h=1", now=100)
        self.st.raw("usage 5h=1", now=110)
        self.st.raw("usage 5h=2", now=111)
        self.st.raw("usage 5h=2", now=150)
        self.assertEqual(self.out, ["usage 5h=1", "usage 5h=2", "usage 5h=2"])

    def test_done_while_other_works(self):
        self.ev("a", "UserPromptSubmit", ["ev prompt"])
        self.ev("b", "UserPromptSubmit", ["ev prompt"])
        self.ev("b", "PreToolUse", ["ev tool Bash"])
        del self.out[:]
        self.ev("a", "Stop", ["ev done"])
        self.assertEqual(self.out, ["ev done small", "ev tool Bash"])
        self.ev("b", "Stop", ["ev done"])
        self.assertEqual(self.out[-1], "ev done")

    def test_done_while_other_thinks(self):
        self.ev("a", "UserPromptSubmit", ["ev prompt"])
        self.ev("a", "PreToolUse", ["ev tool Read"])
        self.ev("b", "UserPromptSubmit", ["ev prompt"])
        del self.out[:]
        self.ev("a", "Stop", ["ev done"])
        self.assertEqual(self.out, ["ev done small", "ev prompt"])

    def test_idle_session_does_not_count(self):
        self.ev("a", "SessionStart", ["ev start"])
        self.ev("b", "UserPromptSubmit", ["ev prompt"])
        self.ev("b", "Stop", ["ev done"])
        self.assertEqual(self.out[-1], "ev done")

    def test_stuck_session_expires(self):
        self.ev("a", "UserPromptSubmit", ["ev prompt"])
        self.ev("b", "UserPromptSubmit", ["ev prompt"])
        self.st.sessions["a"].seen -= 700   # lost its Stop
        self.ev("b", "Stop", ["ev done"])
        self.assertEqual(self.out[-1], "ev done")

    def test_perm_names(self):
        self.ev("a", "UserPromptSubmit", ["ev prompt"], proj="tinyclawd")
        self.ev("a", "PermissionRequest", ["ev perm"], proj="tinyclawd", tool="Bash")
        self.assertEqual(self.out[-1], "ev perm tinyclawd")
        self.ev("b", "PreToolUse", ["ev perm"], proj="dimos")
        self.assertEqual(self.out[-1], "ev perm +2")
        # a gets its answer: back to one name
        self.ev("a", "PostToolUse", [], proj="tinyclawd")
        self.assertEqual(self.out[-1], "ev perm dimos")
        # b answered a question, back to thinking
        self.ev("b", "PostToolUse", [], proj="dimos")
        self.assertEqual(self.out[-1], "ev prompt")

    def test_perm_answered_goes_back_to_work(self):
        self.ev("a", "PreToolUse", ["ev tool Bash"], proj="x")
        self.ev("a", "Notification", ["ev perm"], proj="x")
        self.ev("a", "PostToolUse", [], proj="x")
        self.assertEqual(self.out, ["ev tool Bash", "ev perm x", "ev tool Bash"])

    def test_sessions_count(self):
        self.ev("a", "UserPromptSubmit", ["ev prompt"])
        self.assertNotIn("sessions 1", self.all)   # one session is the normal case
        self.ev("b", "UserPromptSubmit", ["ev prompt"])
        self.ev("c", "PermissionRequest", ["ev perm"])
        self.ev("c", "PostToolUse", [])
        self.ev("a", "Stop", ["ev done"])
        self.ev("c", "Stop", ["ev done"])
        self.ev("b", "Stop", ["ev done"])
        self.assertEqual([l for l in self.all if l.startswith("sessions")],
                         ["sessions 2", "sessions 3", "sessions 2", "sessions 1"])

    def test_end_while_busy_resumes_other(self):
        self.ev("a", "UserPromptSubmit", ["ev prompt"])
        self.ev("b", "PreToolUse", ["ev tool Grep"])
        self.ev("a", "PreToolUse", ["ev tool Bash"])
        self.ev("a", "SessionEnd", ["ev end"])
        self.assertEqual(self.out[-1], "ev tool Grep")

    def test_hook_message_project(self):
        m = json.loads(tc.hook_message(hook("PermissionRequest", tool_name="Bash",
                                            cwd="/home/me/My Code/tiny clawd/")))
        self.assertEqual((m["proj"], m["tool"]), ("tinyclawd", "Bash"))
        m = json.loads(tc.hook_message(hook("Stop", cwd="/home/me/" + "x" * 30)))
        self.assertEqual(m["proj"], "x" * 16)
        self.assertEqual(tc.project("/"), "")
        self.assertEqual(tc.project(None), "")

    def test_prune(self):
        self.ev("a", "SubagentStart", aid="1")
        self.st.sessions["a"].seen -= 4000
        self.st.prune(time.time())
        self.assertEqual(self.out, ["ev sub 1", "ev sub 0"])


def player(status, artist, title, name="spotify"):
    return "%s\t%s\t%s\t%s\n" % (status, name, artist, title)


class MusicWatch(unittest.TestCase):
    def setUp(self):
        self.out = []
        self.now = ""   # what playerctl prints right now
        self.m = tc.Music(self.out.append, lambda: self.now)

    def at(self, t, text):
        self.now = text
        self.m.poll(now=1000 + t)

    def test_start_and_track_change(self):
        self.at(0, player("Playing", "Daft Punk", "One More Time"))
        self.at(2, player("Playing", "Daft Punk", "One More Time"))
        self.at(4, player("Playing", "Daft Punk", "Aerodynamic"))
        self.assertEqual(self.out, ["music 1 Daft Punk - One More Time",
                                    "music 1 Daft Punk - Aerodynamic"])

    def test_short_pause_no_flicker(self):
        self.at(0, player("Playing", "A", "B"))
        self.at(2, player("Paused", "A", "B"))
        self.at(4, player("Paused", "A", "B"))
        self.at(6, player("Playing", "A", "B"))
        self.assertEqual(self.out, ["music 1 A - B"])

    def test_stop_after_5s(self):
        self.at(0, player("Playing", "A", "B"))
        self.at(2, player("Paused", "A", "B"))
        self.at(4, "")
        self.assertEqual(self.out, ["music 1 A - B"])
        self.at(6, "")
        self.assertEqual(self.out, ["music 1 A - B", "music 0"])
        self.at(8, "")
        self.assertEqual(len(self.out), 2)
        self.at(10, player("Playing", "A", "B"))
        self.assertEqual(self.out[-1], "music 1 A - B")

    def test_first_playing_player_wins(self):
        text = player("Paused", "X", "Y", "chromium") + player("Playing", "", "a video", "firefox")
        self.at(0, text)
        self.assertEqual(self.out, ["music 1 a video"])

    def test_weird_titles(self):
        self.assertEqual(tc.music_line(player("Playing", "Sigur Rós", "Hoppípolla")),
                         "music 1 Sigur Rs - Hopppolla")
        self.assertEqual(tc.music_line(player("Playing", "", "  lo\x07fi \U0001f3b5  beats\x1b ")),
                         "music 1 lofi beats")
        self.assertEqual(tc.music_line(player("Playing", "山下", "曲")), "music 1")
        self.assertEqual(tc.music_line(player("Playing", "a", "b\tc")), "music 1 a - bc")
        long = tc.music_line(player("Playing", "x" * 40, "y" * 40))
        self.assertEqual(long, "music 1 " + "x" * 40 + " - yyyyy")
        self.assertIsNone(tc.music_line("garbage\n\n"))

    def test_no_players(self):
        self.at(0, "")
        self.at(10, "")
        self.assertEqual(self.out, [])
        self.assertIsNone(tc.music_line(None))

    def test_missing_playerctl(self):
        calls = []
        m = tc.Music(self.out.append, lambda: calls.append(1))   # returns None
        m.poll()
        m.poll()
        m.loop()   # returns right away
        self.assertEqual((self.out, len(calls), m.off), ([], 1, True))

    def test_resent_on_connect(self):
        d = tc.Daemon(stdout=True, path="/tmp/tinyclawd-test-unused.sock", player=lambda: "")
        sent = []
        d.link.send = sent.append
        d.on_connect()
        self.assertNotIn("music 1 A - B", sent)
        d.music.line = "music 1 A - B"
        d.on_connect()
        self.assertEqual(sent[-1], "music 1 A - B")


class Typing(unittest.TestCase):
    def setUp(self):
        self.out = []
        self.k = tc.Keys(self.out.append, source=None, env={"DISPLAY": ":1"})

    def run_until(self, t0, t1, step=0.25):
        t = t0
        while t <= t1 + 1e-9:
            self.k.tick(now=1000 + t)
            t += step

    def type_keys(self, t0, t1, per_s):
        # key presses at a steady rate, ticking like the daemon does
        t = t0
        while t < t1:
            self.k.press(now=1000 + t)
            self.k.tick(now=1000 + t)
            t += 1.0 / per_s

    def test_burst_then_pause(self):
        self.type_keys(0, 5, 6)
        self.assertEqual(self.out[0], "typing 1")
        self.assertEqual(self.out[-1], "typing 6")
        self.run_until(5, 6.5)
        self.assertNotIn("typing 0", self.out)   # slows down, but still inside the 3 s
        self.run_until(7, 8.5)
        self.assertEqual(self.out[-1], "typing 0")
        n = len(self.out)
        self.run_until(9, 20)
        self.assertEqual(len(self.out), n)   # stopped stays quiet

    def test_rate_limited(self):
        self.type_keys(0, 10, 15)
        sent = [l for l in self.out if l != "typing 0"]
        self.assertEqual(sent[-1], "typing 15")
        # never more than 2 lines a second, and no line for every key
        self.assertLessEqual(len(self.out), 10 * 2)
        self.assertLess(len(self.out), 15)

    def test_small_changes_wait(self):
        self.type_keys(0, 5, 6)
        n = len(self.out)
        self.type_keys(5, 5.6, 7)   # a little faster: not right away
        self.assertEqual(set(self.out[n:]), set(self.out[n:]) & {"typing 6"})
        self.type_keys(5.6, 9, 7)
        self.assertEqual(self.out[-1], "typing 7")

    def test_keepalive_and_resend(self):
        self.type_keys(0, 12, 4)
        self.assertGreaterEqual(self.out.count("typing 4"), 2)
        d = tc.Daemon(stdout=True, path="/tmp/tinyclawd-test-unused.sock", player=lambda: "")
        sent = []
        d.link.send = sent.append
        d.on_connect()
        self.assertFalse(any(l.startswith("typing") for l in sent))
        d.keys.rate = 4
        d.on_connect()
        self.assertEqual(sent[-1], "typing 4")

    def test_turned_off(self):
        self.assertIsNone(self.k.why_off())
        for env in ({"DISPLAY": ":1", "TINYCLAWD_NO_KEYS": "1"}, {},
                    {"DISPLAY": ":1", "XDG_SESSION_TYPE": "wayland"}):
            k = tc.Keys(self.out.append, source=lambda f: self.fail("ran"), env=env)
            self.assertTrue(k.why_off())
            k.loop()   # returns right away
        d = tc.Daemon(stdout=True, path="/tmp/tinyclawd-test-unused.sock", player=lambda: "",
                      no_keys=True)
        self.assertTrue(d.keys.why_off())
        self.assertEqual(self.out, [])

    def test_missing_xinput(self):
        def gone(on_press):
            raise FileNotFoundError("xinput")
        k = tc.Keys(self.out.append, source=gone, env={"DISPLAY": ":1"})
        k.watch()
        self.assertTrue(k.off)
        k.loop()
        self.assertEqual(self.out, [])

    def test_only_key_presses_counted(self):
        text = ("EVENT type 13 (RawKeyPress)\n    device: 3 (14)\n    detail: 38\n"
                "EVENT type 14 (RawKeyRelease)\n    device: 3 (14)\n    detail: 38\n"
                "EVENT type 2 (KeyPress)\n    device: 11 (11)\n    detail: 38\n"
                "EVENT type 13 (RawKeyPress)\n    device: 3 (14)\n    detail: 40\n")
        fake = tempfile.NamedTemporaryFile("w", suffix=".sh", delete=False)
        fake.write("#!/bin/sh\nprintf '%s'\n" % text.replace("%", "%%"))
        fake.close()
        os.chmod(fake.name, 0o755)
        bindir = tempfile.mkdtemp()
        os.symlink(fake.name, os.path.join(bindir, "xinput"))
        old = os.environ["PATH"]
        os.environ["PATH"] = bindir + os.pathsep + old
        try:
            got = []
            tc.xinput_keys(lambda: got.append(1))
        finally:
            os.environ["PATH"] = old
        self.assertEqual(len(got), 2)


def tone(hz, amp=0.5, n=tc.EQ_BLOCK, start=0):
    import math
    return [amp * math.sin(2 * math.pi * hz * (start + i) / tc.EQ_RATE) for i in range(n)]


def raw(samples):
    import array
    return array.array("h", (int(v * 32767) for v in samples)).tobytes()


class FakeAudio:
    """stands in for parec: a tone, 10 ms a block, until closed"""

    def __init__(self, hz=60, blocks=None):
        self.hz = hz
        self.blocks = blocks   # None = forever
        self.closed = threading.Event()
        self.n = 0

    def read(self, n):
        if self.closed.wait(0.01) or (self.blocks is not None and self.n >= self.blocks):
            return b""
        self.n += 1
        # louder and softer so the levels change
        return raw(tone(self.hz, 0.6 if self.n % 2 else 0.1, n // 2, self.n * n))

    def close(self):
        self.closed.set()


def wait_for(cond, timeout=2):
    end = time.time() + timeout
    while time.time() < end:
        if cond():
            return True
        time.sleep(0.01)
    return False


class Equalizer(unittest.TestCase):
    def feed(self, b, hz, amp=0.5, blocks=6):
        lv = None
        for k in range(blocks):
            lv = b.levels(tone(hz, amp, start=k * tc.EQ_BLOCK))
        return lv

    def test_bands_numpy(self):
        try:
            import numpy  # noqa: F401
        except ImportError:
            self.skipTest("no numpy")
        low = self.feed(tc.Bands(True), 60)
        self.assertEqual(low.index(max(low)), 0)
        high = self.feed(tc.Bands(True), 3500)
        self.assertEqual(high.index(max(high)), 7)
        mid = self.feed(tc.Bands(True), 500)
        self.assertEqual(mid.index(max(mid)), 3)
        self.assertTrue(all(0 <= v <= 9 for v in low + high + mid))

    def test_bands_without_numpy(self):
        low = self.feed(tc.Bands(False), 60)
        high = self.feed(tc.Bands(False), 3500)
        self.assertLess(low.index(max(low)), 3)
        self.assertGreater(high.index(max(high)), 4)

    def test_silence_is_empty(self):
        for np_ in (True, False):
            try:
                b = tc.Bands(np_)
                self.assertEqual(self.feed(b, 60, amp=0), [0] * 8)
            except ImportError:
                pass

    def test_auto_gain(self):
        b = tc.Bands(False)
        loud = self.feed(b, 300, 0.8, blocks=10)
        right_after = b.levels(tone(300, 0.03))
        self.assertLess(max(right_after), max(loud))
        # 20 s of the quiet song: the gain comes back and the bars fill again
        later = self.feed(b, 300, 0.03, blocks=250)
        self.assertGreaterEqual(max(later), max(loud) - 1)
        self.assertGreater(max(later), max(right_after))

    def test_samples(self):
        self.assertEqual(tc.to_samples(raw([0, 0.5, -0.5]) + b"\x01"),
                         [0.0, 16383 / 32768, -16383 / 32768])

    def test_rate_limit_and_changes_only(self):
        out = []
        e = tc.Eq(out.append, lambda: None)
        e.on = True
        self.assertTrue(e.send([1, 2, 3, 4, 5, 6, 7, 8], now=100.0))
        self.assertFalse(e.send([2, 2, 3, 4, 5, 6, 7, 8], now=100.03))   # too soon
        self.assertTrue(e.send([2, 2, 3, 4, 5, 6, 7, 8], now=100.1))
        self.assertFalse(e.send([2, 2, 3, 4, 5, 6, 7, 8], now=100.5))   # same as before
        self.assertEqual(out, ["eq 12345678", "eq 22345678"])
        e.on = False
        self.assertFalse(e.send([0] * 8, now=200.0))

    def test_follows_the_music(self):
        out = []
        lock = threading.Lock()

        def put(line):
            with lock:
                out.append(line)
        audio = FakeAudio()
        e = tc.Eq(put, lambda: audio)
        m = tc.Music(put, lambda: self.now, e)
        self.now = ""
        m.poll(now=1000)
        self.assertFalse(e.on)
        self.now = player("Playing", "A", "B")
        m.poll(now=1002)
        self.assertTrue(wait_for(lambda: len(out) >= 3))
        self.assertEqual(out[0], "music 1 A - B")
        self.assertTrue(all(l.startswith("eq ") and len(l) == 11 for l in out[1:]))
        self.now = ""
        m.poll(now=1010)
        self.assertTrue(audio.closed.is_set())
        time.sleep(0.1)
        with lock:
            self.assertEqual(out[-1], "music 0")
        self.assertFalse(e.on)

    def test_env_switch(self):
        calls = []
        os.environ["TINYCLAWD_NO_EQ"] = "1"
        try:
            e = tc.Eq(lambda l: None, lambda: calls.append(1))
        finally:
            del os.environ["TINYCLAWD_NO_EQ"]
        e.start()
        self.assertEqual((calls, e.on), ([], False))

    def test_missing_tools(self):
        self.assertIsNone(tc.parec_cmd(lambda name: None))
        self.assertEqual(tc.parec_cmd(lambda name: "/usr/bin/parec")[:3],
                         ["parec", "-d", "@DEFAULT_MONITOR@"])
        calls = []
        out = []
        e = tc.Eq(out.append, lambda: calls.append(1))   # no capture tool
        e.start()
        e.start()
        self.assertEqual((calls, e.on, e.off, out), ([1], False, True, []))

    def test_capture_ends(self):
        out = []
        e = tc.Eq(out.append, lambda: FakeAudio(blocks=3))
        e.start()
        self.assertTrue(wait_for(lambda: not e.on))
        e.start()   # next song tries again
        self.assertTrue(wait_for(lambda: not e.on))


class FakePort(unittest.TestCase):
    """daemon talking to a pty pair instead of the pet"""

    def setUp(self):
        try:
            import serial  # noqa: F401
        except ImportError:
            self.skipTest("pyserial missing")
        self.master, slave = os.openpty()
        self.slave_name = os.ttyname(slave)
        self.slave = slave
        self.sock = os.path.join(tempfile.mkdtemp(), "t.sock")
        self.stop = threading.Event()
        self.d = tc.Daemon(port=self.slave_name, path=self.sock, player=lambda: "", no_keys=True)
        self.t = threading.Thread(target=self.d.run, args=(self.stop,), daemon=True)
        self.t.start()
        self.buf = b""

    def tearDown(self):
        self.stop.set()
        self.t.join(3)
        os.close(self.master)
        os.close(self.slave)

    def expect(self, line, timeout=4):
        end = time.time() + timeout
        while time.time() < end:
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                if raw.decode().strip() == line or (line.endswith("*") and
                                                    raw.decode().startswith(line[:-1])):
                    return True
            r, _, _ = select.select([self.master], [], [], 0.1)
            if r:
                self.buf += os.read(self.master, 1024)
        self.fail("never saw %r (buffer %r)" % (line, self.buf))

    def test_hello_forward_and_reboot(self):
        self.expect("hello 1")
        self.expect("time *")
        tc.send_to_daemon("ev perm", path=self.sock)
        self.expect("ev perm")
        tc.send_to_daemon(tc.hook_message(hook("PreToolUse", tool_name="Grep")), path=self.sock)
        self.expect("ev tool Grep")
        # garbage from the socket must not kill it
        tc.send_to_daemon(["{not json", "{\"lines\": 5}", "\x00\xff"], path=self.sock)
        # esp-idf logs then a reboot line from the pet
        os.write(self.master, b"I (312) main: hi\nok\nboot tinyclawd 1\n")
        self.expect("hello 1")
        tc.send_to_daemon("ev done", path=self.sock)
        self.expect("ev done")
        self.assertTrue(self.t.is_alive())


if __name__ == "__main__":
    unittest.main(verbosity=2)

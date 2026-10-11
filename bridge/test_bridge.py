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
        self.out = []
        self.st = tc.State(self.out.append)

    def ev(self, sid, event, lines=(), aid="", ts=None):
        self.st.hook({"sid": sid, "event": event, "lines": list(lines), "aid": aid,
                      "ts": ts or time.time()})

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

    def test_prune(self):
        self.ev("a", "SubagentStart", aid="1")
        self.st.sessions["a"].seen -= 4000
        self.st.prune(time.time())
        self.assertEqual(self.out, ["ev sub 1", "ev sub 0"])


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
        self.d = tc.Daemon(port=self.slave_name, path=self.sock)
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

#!/usr/bin/env python3
# tests for setup.py and install_hooks.py: python3 bridge/test_setup.py
# everything runs in a throwaway HOME with a fake systemctl, your real ~/.claude is never read

import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
REAL_HOME = os.path.expanduser("~")
# before the imports, so even their module level paths point at a temp dir
os.environ["HOME"] = tempfile.mkdtemp(prefix="tinyclawd-test-home-")
for var in ("CLAUDE_CONFIG_DIR", "XDG_CONFIG_HOME"):
    os.environ.pop(var, None)
sys.path.insert(0, HERE)
import install_hooks as ih  # noqa: E402
import setup  # noqa: E402

assert not str(ih.SETTINGS).startswith(REAL_HOME + "/.claude")

USER_HOOKS = {
    "Stop": [{"hooks": [{"type": "command", "command": "notify-send done"}]}],
    "PreToolUse": [{"matcher": "Bash", "hooks": [{"type": "command", "command": "my-guard.sh"}]}],
}
STATUS_SCRIPT = """#!/bin/bash
# my status line
input=$(cat)
model=$(echo "$input" | jq -r .model.display_name)
echo "$model"
"""


class Home:
    """a fake HOME with a fake systemctl on PATH"""

    def __init__(self, systemd=True):
        self.dir = tempfile.mkdtemp(prefix="tinyclawd test home ")   # a space on purpose
        self.bin = os.path.join(self.dir, "bin")
        os.makedirs(self.bin)
        fake = os.path.join(self.bin, "systemctl")
        with open(fake, "w") as f:
            f.write('#!/bin/sh\necho "$@" >> "$HOME/systemctl.log"\n%s\n'
                    % ("exit 0" if systemd else 'case "$*" in *show-environment*) exit 1;; esac'))
        os.chmod(fake, 0o755)
        self.claude = os.path.join(self.dir, ".claude")
        self.settings = os.path.join(self.claude, "settings.json")
        self.unit = os.path.join(self.dir, ".config/systemd/user/tinyclawd-daemon.service")

    def env(self):
        env = dict(os.environ, HOME=self.dir, PATH=self.bin + os.pathsep + os.environ["PATH"])
        for var in ("CLAUDE_CONFIG_DIR", "XDG_CONFIG_HOME"):
            env.pop(var, None)
        return env

    def run(self, *args, ok=True):
        p = subprocess.run([sys.executable, os.path.join(HERE, "setup.py")] + list(args),
                           capture_output=True, text=True, env=self.env(), timeout=30)
        if ok and p.returncode != 0:
            raise AssertionError(p.stdout + p.stderr)
        return p

    def write_settings(self, data):
        os.makedirs(self.claude, exist_ok=True)
        with open(self.settings, "w") as f:
            f.write(data if isinstance(data, str) else json.dumps(data, indent=2))

    def read_settings(self):
        with open(self.settings) as f:
            return json.load(f)

    def backups(self):
        out = []
        for root, _, files in os.walk(self.dir):
            out += [f for f in files if ".bak-tinyclawd-" in f]
        return out

    def files(self):
        out = set()
        for root, _, files in os.walk(self.dir):
            for f in files:
                p = os.path.relpath(os.path.join(root, f), self.dir)
                if not p.startswith("bin") and p != "systemctl.log" and ".bak-tinyclawd-" not in p:
                    out.add(p)
        return out

    def cleanup(self):
        shutil.rmtree(self.dir, ignore_errors=True)


def our_commands(settings):
    out = []
    for ev, groups in settings.get("hooks", {}).items():
        for g in groups:
            out += [(ev, h["command"]) for h in g["hooks"] if "tinyclawd.py" in h["command"]]
    return out


class Merge(unittest.TestCase):
    def test_add_keeps_yours_and_is_idempotent(self):
        s = {"model": "opus", "hooks": json.loads(json.dumps(USER_HOOKS))}
        n = ih.merge_hooks(s)
        self.assertEqual(n, len(ih.EVENTS) + len(ih.SYNC))
        self.assertEqual(s["model"], "opus")
        self.assertEqual(s["hooks"]["Stop"][0], USER_HOOKS["Stop"][0])
        self.assertEqual(s["hooks"]["PreToolUse"][0], USER_HOOKS["PreToolUse"][0])
        self.assertEqual(len(our_commands(s)), n)
        self.assertEqual(ih.merge_hooks(s), 0)
        self.assertTrue(s["hooks"]["UserPromptExpansion"][0]["hooks"][0].get("async") is None)
        self.assertTrue(s["hooks"]["Stop"][1]["hooks"][0]["async"])

    def test_remove_gives_back_the_original(self):
        s = {"hooks": json.loads(json.dumps(USER_HOOKS))}
        ih.merge_hooks(s)
        ih.merge_hooks(s, remove=True)
        self.assertEqual(s, {"hooks": USER_HOOKS})
        s = {}
        ih.merge_hooks(s)
        ih.merge_hooks(s, remove=True)
        self.assertEqual(s, {})

    def test_old_paths_are_replaced_in_place(self):
        old = "/usr/bin/python3 /old/place/bridge/tinyclawd.py hook"
        s = {"hooks": {"Stop": [
            {"hooks": [{"type": "command", "command": old, "async": True}]},
            {"hooks": [{"type": "command", "command": "notify-send done"}]},
        ], "Gone": [{"hooks": [{"type": "command", "command": old}]}]}}
        ih.merge_hooks(s)
        stop = s["hooks"]["Stop"]
        self.assertEqual(len(stop), 2)
        self.assertIn(str(ih.TC), stop[0]["hooks"][0]["command"])
        self.assertEqual(stop[1]["hooks"][0]["command"], "notify-send done")
        self.assertNotIn("Gone", s["hooks"])   # an event we no longer use is cleaned up

    def test_shared_group_keeps_your_hook(self):
        s = {"hooks": {"Stop": [{"hooks": [
            {"type": "command", "command": "notify-send done"},
            {"type": "command", "command": "python3 /x/tinyclawd.py hook"},
        ]}]}}
        ih.merge_hooks(s, remove=True)
        self.assertEqual(s, {"hooks": {"Stop": [{"hooks": [{"type": "command", "command": "notify-send done"}]}]}})

    def test_odd_settings(self):
        s = {"hooks": {"Stop": "not a list"}}
        ih.merge_hooks(s)
        self.assertEqual(s["hooks"]["Stop"], "not a list")
        with self.assertRaises(SystemExit):
            ih.merge_hooks({"hooks": []})

    def test_paths_with_spaces_are_quoted(self):
        old = ih.TC
        ih.TC = ih.Path("/home/me/my stuff/tinyclawd/bridge/tinyclawd.py")
        try:
            cmd = ih.command("hook")
        finally:
            ih.TC = old
        self.assertIn("'/home/me/my stuff/tinyclawd/bridge/tinyclawd.py' hook", cmd)
        self.assertTrue(ih.ours({"command": cmd}))


class StatusScript(unittest.TestCase):
    def test_insert_and_remove(self):
        new = setup.patch_script(STATUS_SCRIPT, remove=False)
        lines = new.splitlines()
        self.assertEqual(lines[2], "input=$(cat)")
        self.assertIn("tinyclawd.py usage", lines[3])
        self.assertTrue(lines[3].startswith("printf '%s' \"$input\" | "))
        self.assertTrue(lines[3].endswith(">/dev/null 2>&1 &"))
        self.assertEqual(setup.patch_script(new, remove=False), new)   # twice = once
        self.assertEqual(setup.patch_script(new, remove=True), STATUS_SCRIPT)

    def test_other_reads(self):
        for read, var in (('  json="$(cat)"', "json"), ("data=$(cat -)  # stdin", "data")):
            new = setup.patch_script("#!/bin/sh\n" + read + "\necho hi\n", remove=False)
            self.assertIn('"$%s" |' % var, new.splitlines()[2])
        self.assertIsNone(setup.patch_script("#!/bin/sh\njq -r .model\n", remove=False))
        self.assertEqual(setup.patch_script("#!/bin/sh\ninput=$(cat)", remove=False).count("\n"), 3)

    def test_find_script(self):
        d = tempfile.mkdtemp(prefix="tinyclawd status ")
        sh = os.path.join(d, "status line.sh")
        plain = os.path.join(d, "statusline")
        py = os.path.join(d, "status.py")
        for path, text in ((sh, STATUS_SCRIPT), (plain, STATUS_SCRIPT), (py, "#!/usr/bin/env python3\n")):
            with open(path, "w") as f:
                f.write(text)
        q = lambda p: "'%s'" % p
        self.assertEqual(str(setup.find_script("bash " + q(sh))), sh)
        self.assertEqual(str(setup.find_script(q(plain))), plain)
        self.assertIsNone(setup.find_script("python3 " + q(py)))
        self.assertIsNone(setup.find_script("echo hi"))
        self.assertIsNone(setup.find_script("echo 'unclosed"))
        shutil.rmtree(d)

    def test_unit_quotes_paths(self):
        self.assertEqual(setup.sd_quote("/a b/50%/$x"), '"/a b/50%%/$$x"')
        text = setup.unit_text("/dev/ttyUSB0")
        self.assertIn('"daemon" "--port" "/dev/ttyUSB0"', text)
        self.assertNotIn("@", text)


class SetupInAFakeHome(unittest.TestCase):
    def setUp(self):
        self.h = Home()

    def tearDown(self):
        self.h.cleanup()

    def test_fresh_install_and_remove(self):
        h = self.h
        out = h.run("--dry-run").stdout
        self.assertIn("nothing was changed", out)
        self.assertEqual(h.files(), set())
        h.run()
        s = h.read_settings()
        self.assertEqual(len(our_commands(s)), len(ih.EVENTS) + len(ih.SYNC))
        self.assertTrue(setup.is_ours(s["statusLine"]["command"]))
        self.assertTrue(os.path.exists(h.unit))
        self.assertTrue(os.path.exists(os.path.join(h.claude, "commands/clawd.md")))
        with open(h.unit) as f:
            self.assertIn('tinyclawd.py" "daemon"', f.read())
        with open(os.path.join(h.dir, "systemctl.log")) as f:
            self.assertIn("--user restart tinyclawd-daemon", f.read())
        before = h.backups()
        self.assertIn("already up to date", h.run().stdout)
        self.assertEqual(h.backups(), before)   # a re-run with nothing to do makes no backups
        h.run("--remove")
        self.assertEqual(h.read_settings(), {})
        self.assertEqual(h.files(), {".claude/settings.json"})

    def test_existing_settings_and_status_script(self):
        h = self.h
        script = os.path.join(h.claude, "my status.sh")
        os.makedirs(h.claude)
        with open(script, "w") as f:
            f.write(STATUS_SCRIPT)
        original = {"model": "opus", "hooks": USER_HOOKS,
                    "statusLine": {"type": "command", "command": "bash '%s'" % script}}
        h.write_settings(original)
        h.run()
        s = h.read_settings()
        self.assertEqual(s["statusLine"], original["statusLine"])
        with open(script) as f:
            patched = f.read()
        self.assertIn("tinyclawd.py usage", patched)
        self.assertEqual(len(h.backups()), 2)   # settings.json and the script

        # the patched script still prints its line and sends usage to the daemon socket
        sock_path = os.path.join(tempfile.mkdtemp(), "t.sock")
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.bind(sock_path)
        srv.listen(1)
        srv.settimeout(5)
        env = dict(h.env(), TINYCLAWD_SOCK=sock_path)
        if shutil.which("jq"):
            js = {"model": {"display_name": "Opus"}, "context_window": {"used_percentage": 42}}
            p = subprocess.run(["bash", script], input=json.dumps(js), capture_output=True,
                               text=True, env=env, timeout=10)
            self.assertEqual(p.stdout.strip(), "Opus")
            conn, _ = srv.accept()
            self.assertEqual(conn.recv(100).decode().strip(), "usage ctx=42")
            conn.close()
        srv.close()

        h.run("--remove")
        self.assertEqual(h.read_settings(), original)
        with open(script) as f:
            self.assertEqual(f.read(), STATUS_SCRIPT)

    def test_status_line_we_can_not_patch(self):
        h = self.h
        original = {"statusLine": {"type": "command", "command": "echo hello"}}
        h.write_settings(original)
        out = h.run().stdout
        self.assertIn("not a shell script", out)
        self.assertIn("tinyclawd.py usage", out)   # tells you the line to add
        self.assertEqual(h.read_settings()["statusLine"], original["statusLine"])
        h.run("--remove")
        self.assertEqual(h.read_settings(), original)

    def test_bad_json_changes_nothing(self):
        h = self.h
        h.write_settings('{"hooks": {,}')
        p = h.run(ok=False)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("not valid json", p.stdout + p.stderr)
        with open(h.settings) as f:
            self.assertEqual(f.read(), '{"hooks": {,}')
        self.assertEqual(h.backups(), [])
        self.assertFalse(os.path.exists(h.unit))

    def test_dry_run_changes_nothing(self):
        h = self.h
        h.write_settings({"hooks": USER_HOOKS})
        h.run("--dry-run")
        self.assertEqual(h.read_settings(), {"hooks": USER_HOOKS})
        self.assertEqual(h.files(), {".claude/settings.json"})
        self.assertEqual(h.backups(), [])

    def test_symlinked_settings_stay_a_symlink(self):
        h = self.h
        real = os.path.join(h.dir, "dotfiles", "settings.json")
        os.makedirs(os.path.dirname(real))
        with open(real, "w") as f:
            json.dump({"model": "opus"}, f)
        os.makedirs(h.claude)
        os.symlink(real, h.settings)
        h.run()
        self.assertTrue(os.path.islink(h.settings))
        with open(real) as f:
            self.assertIn("hooks", json.load(f))

    def test_no_systemd(self):
        h = Home(systemd=False)
        try:
            out = h.run().stdout
            self.assertIn("start the daemon yourself", out)
            self.assertFalse(os.path.exists(h.unit))
            self.assertTrue(our_commands(h.read_settings()))
        finally:
            h.cleanup()


if __name__ == "__main__":
    try:
        unittest.main(verbosity=2)
    finally:
        shutil.rmtree(os.environ["HOME"], ignore_errors=True)

#!/bin/bash
# prints what to add to claude code. it does not change any of your files.
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
PY="$(command -v python3)"
[ -x /usr/bin/python3 ] && PY=/usr/bin/python3
CMD="$PY $DIR/tinyclawd.py hook"

if ! "$PY" -c "import serial" 2>/dev/null; then
    echo "pyserial is missing for $PY, run: pip install --user pyserial"
    echo
fi

ev() {
    # $1 = event, $2 = matcher or empty
    local m=""
    [ -n "$2" ] && m="\"matcher\": \"$2\", "
    printf '    "%s": [{ %s"hooks": [{ "type": "command", "command": "%s", "async": true, "timeout": 5 }] }]' "$1" "$m" "$CMD"
}

echo "1) run: $PY $DIR/install_hooks.py   (adds them for you, keeps your hooks, makes a backup)"
echo "   or add this to the \"hooks\" object in ~/.claude/settings.json by hand:"
echo
echo '  "hooks": {'
{
    ev SessionStart
    echo ","; ev SessionEnd
    echo ","; ev UserPromptSubmit
    echo ","; ev PreToolUse "*"
    echo ","; ev PostToolUse "*"
    echo ","; ev PostToolUseFailure "*"
    echo ","; ev PermissionRequest "*"
    echo ","; ev Notification
    echo ","; ev Stop
    echo ","; ev StopFailure
    echo ","; ev SubagentStart
    echo ","; ev SubagentStop
    echo ","; ev PreCompact
    echo
}
echo '  }'
echo
echo "2) in your status line script, right after the line that reads stdin (input=\$(cat)), add:"
echo
echo "   printf '%s' \"\$input\" | $PY $DIR/tinyclawd.py usage >/dev/null 2>&1 &"
echo
echo "3) run the daemon, either by hand:"
echo
echo "   $PY $DIR/tinyclawd.py daemon"
echo
echo "   or as a user service:"
echo
echo "   mkdir -p ~/.config/systemd/user"
echo "   sed \"s|@DIR@|$DIR|; s|@PY@|$PY|\" $DIR/tinyclawd-daemon.service > ~/.config/systemd/user/tinyclawd-daemon.service"
echo "   systemctl --user daemon-reload && systemctl --user enable --now tinyclawd-daemon"

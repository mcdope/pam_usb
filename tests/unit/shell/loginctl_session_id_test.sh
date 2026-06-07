#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
# Tests for the loginctl property-extraction pipeline used by
# pusb_get_loginctl_session_property() in src/local.c.
#
# The old code extracted a session ID from "loginctl user-status" output and
# then queried that specific session. When a user had multiple sessions (e.g. a
# local desktop session and an SSH session), it always picked the first scope
# line, which could be the SSH session, causing local sudo to be denied
# (issue #430).
#
# The fix calls "loginctl show-session auto -p <property>", which systemd-logind
# resolves to the session the calling process belongs to (via cgroup membership),
# regardless of how many other sessions the user has open.

pass=0
fail=0

assert_eq() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$actual" = "$expected" ]; then
        printf '[PASS] %s\n' "$desc"
        pass=$((pass + 1))
    else
        printf '[FAIL] %s — expected "%s", got "%s"\n' "$desc" "$expected" "$actual"
        fail=$((fail + 1))
    fi
}

# --- awk KEY=value extraction (uses real awk to mirror the production pipeline) ---
# src/local.c pipes loginctl output through: awk -F= '{print $2}'

assert_eq "awk: Remote=no"        "no"         "$(printf 'Remote=no\n'        | awk -F= '{print $2}')"
assert_eq "awk: Remote=yes"       "yes"        "$(printf 'Remote=yes\n'       | awk -F= '{print $2}')"
assert_eq "awk: TTY field"        "tty7"       "$(printf 'TTY=tty7\n'         | awk -F= '{print $2}')"
assert_eq "awk: multi-= value"    "/dev/pts/0" "$(printf 'TTY=/dev/pts/0=1\n' | awk -F= '{print $2}')"
assert_eq "awk: empty input"      ""           "$(printf '' | awk -F= '{print $2}')"

# --- regression test for issue #430: multi-session user picks wrong session ---
#
# Scenario: user has session c2 (SSH, Remote=yes) and session c6 (local, Remote=no).
# Old code: sed picked the first scope line (c2 = SSH), returned "yes" → denied.
# New code: loginctl show-session auto resolves by calling process → returns "no" → allowed.

MOCK_DIR="$(mktemp -d)"
trap 'rm -rf "$MOCK_DIR"' EXIT

# Write a mock loginctl that simulates the multi-session scenario.
# case "$*" is sensitive to argument order; the arms below must match exactly
# the argument sequences produced by the production command in local.c and by
# the old-pipeline demonstration below.
cat > "$MOCK_DIR/loginctl" << 'EOF'
#!/usr/bin/env bash
# Mock loginctl for issue #430 regression test.
# "show-session auto -p Remote" → calling-process session → local → Remote=no
# "show-session auto -p TTY"    → calling-process session → TTY=tty7
# "show-session c2 -p Remote"   → explicit SSH session → Remote=yes
# "user-status"                 → lists SSH session scope (c2) first, then local (c6)
case "$*" in
    "show-session auto -p Remote")
        echo "Remote=no"
        ;;
    "show-session auto -p TTY")
        echo "TTY=tty7"
        ;;
    "show-session c2 -p Remote")
        echo "Remote=yes"
        ;;
    "user-status")
        cat <<'USTAT'
testuser (1000)
           State: active
        Sessions: c2 *c6
          Unit: user-1000.slice
          ├─session-c2.scope
          └─session-c6.scope
USTAT
        ;;
    *)
        exit 1
        ;;
esac
EOF
chmod +x "$MOCK_DIR/loginctl"

# Prepend mock dir to PATH so it shadows the real loginctl.
OLD_PATH="$PATH"
export PATH="$MOCK_DIR:$PATH"

# New command (as used by pusb_get_loginctl_session_property after the fix).
# "auto" resolves to the calling process's session — not the first scope line.
NEW_RESULT="$(loginctl show-session auto -p Remote 2>/dev/null | awk -F= '{print $2}')"
assert_eq "regression #430: auto show-session returns calling-process session (Remote=no)" "no" "$NEW_RESULT"

# Also verify the TTY path (pusb_get_tty_by_loginctl) through the same mock.
NEW_TTY="$(loginctl show-session auto -p TTY 2>/dev/null | awk -F= '{print $2}')"
assert_eq "regression #430: auto show-session returns TTY for calling-process session" "tty7" "$NEW_TTY"

# Demonstrate that the old approach (extract session ID from user-status, pick first)
# would have returned "yes", confirming the bug that this fix addresses.
# Pure-Bash regex avoids the SIGPIPE risk that "sed -n ...;q" causes under
# set -o pipefail when the producer is still writing as sed quits early.
USTAT_OUTPUT="$(loginctl user-status)"
OLD_SESSION_ID=""
if [[ "$USTAT_OUTPUT" =~ session-([a-zA-Z0-9]+)\.scope ]]; then
    OLD_SESSION_ID="${BASH_REMATCH[1]}"
fi
OLD_RESULT=""
if [ -n "$OLD_SESSION_ID" ]; then
    OLD_RESULT="$(loginctl show-session "$OLD_SESSION_ID" -p Remote | awk -F= '{print $2}')"
fi
assert_eq "regression #430 (bug demo): old pipeline picks SSH session (Remote=yes)" "yes" "$OLD_RESULT"

export PATH="$OLD_PATH"

# --- summary ---
echo "---"
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

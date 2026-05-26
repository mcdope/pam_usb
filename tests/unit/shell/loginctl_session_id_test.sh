#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
# Tests that the loginctl session ID extraction pipeline handles both
# alphanumeric IDs (e.g. "c2", "c6" — graphical sessions on modern systemd)
# and pure numeric IDs (e.g. "42" — legacy/TTY sessions).

pass=0
fail=0

extract_session_id() {
    # Mirror the pipeline used in pusb_get_loginctl_session_property (via pusb_get_tty_by_loginctl /
    # pusb_is_loginctl_local). Pure-Bash regex avoids the SIGPIPE risk that sed -n ...;q causes
    # under set -o pipefail when the preceding printf is still writing.
    if [[ "$1" =~ session-([a-zA-Z0-9]+)\.scope ]]; then
        printf '%s\n' "${BASH_REMATCH[1]}"
    fi
}

# Simulate the guarded show-session call: only proceeds when session ID is non-empty.
# Mimics: [ -n "$LOGINCTL_SESSION_ID" ] && loginctl show-session "$LOGINCTL_SESSION_ID" ...
# Here we short-circuit the actual loginctl call and just echo a canned response.
extract_with_guard() {
    local session_id="$1" canned_output="$2"
    if [ -n "$session_id" ]; then
        printf '%s\n' "$canned_output"
    fi
}

# Mirror the awk pipeline used in pusb_get_loginctl_session_property:
#   loginctl show-session "$ID" -p KEY | awk -F= '{print $2}'
# loginctl outputs "KEY=value"; pure-Bash expansion avoids the awk subshell
# and any SIGPIPE risk from a printf|awk pipeline under set -o pipefail.
extract_property_value() {
    if [[ "$1" =~ = ]]; then
        local val="${1#*=}"
        printf '%s\n' "${val%%=*}"
    fi
}

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

# --- test fixtures ---

# Typical graphical session on modern systemd (c-prefixed IDs)
GRAPHICAL_C2='ant (1000)
           Since: Thu 2026-05-19 09:00:00 CEST; 3 days ago
           State: active
        Sessions: c2 *c6
          Unit: user-1000.slice
          ├─session-c2.scope
          └─session-c6.scope'

# Legacy pure-numeric sessions (TTY login, two sessions)
NUMERIC_42='root (0)
           Since: Thu 2026-05-19 09:00:00 CEST
           State: active
        Sessions: 42 *43
          Unit: user-0.slice
          ├─session-42.scope
          └─session-43.scope'

# Single pure-numeric session (only └─ tree connector)
NUMERIC_SINGLE='root (0)
           Since: Thu 2026-05-19 09:00:00 CEST
           State: active
        Sessions: *42
          Unit: user-0.slice
          └─session-42.scope'

# Single alphanumeric session (only └─ tree connector)
ALPHA_SINGLE='ant (1000)
           State: active
        Sessions: *c2
          Unit: user-1000.slice
          └─session-c2.scope'

# Multi-character letter prefix (hypothetical future format, two sessions)
ALPHA_S12='user (1001)
           State: active
        Sessions: s12 *s13
          Unit: user-1001.slice
          ├─session-s12.scope
          └─session-s13.scope'

# No session line at all (e.g. loginctl output truncated)
NO_SESSION='user (1002)
           State: active'

# --- run tests ---

# Session ID extraction
assert_eq "alphanumeric c-prefix, multi-session (c2)" "c2"  "$(extract_session_id "$GRAPHICAL_C2")"
assert_eq "pure numeric, multi-session (42)"          "42"  "$(extract_session_id "$NUMERIC_42")"
assert_eq "pure numeric, single session (42)"         "42"  "$(extract_session_id "$NUMERIC_SINGLE")"
assert_eq "alphanumeric, single session (c2)"         "c2"  "$(extract_session_id "$ALPHA_SINGLE")"
assert_eq "multi-char prefix (s12)"                   "s12" "$(extract_session_id "$ALPHA_S12")"
assert_eq "no session line → empty"                   ""    "$(extract_session_id "$NO_SESSION")"

# Non-empty session ID guard: loginctl show-session is only called when ID was found
assert_eq "guard: non-empty ID passes through"  "tty7" "$(extract_with_guard "c2"  "tty7")"
assert_eq "guard: empty ID suppresses output"   ""     "$(extract_with_guard ""    "tty7")"

# loginctl show-session awk field extraction (awk -F= '{print $2}')
assert_eq "awk: TTY field"           "tty7"       "$(extract_property_value "TTY=tty7")"
assert_eq "awk: Remote field"        "no"         "$(extract_property_value "Remote=no")"
assert_eq "awk: empty input"         ""           "$(extract_property_value "")"
assert_eq "awk: multi-= value (pts)" "/dev/pts/0" "$(extract_property_value "TTY=/dev/pts/0=1")"

# --- summary ---
echo "---"
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]
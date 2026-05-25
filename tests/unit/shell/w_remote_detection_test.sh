#!/usr/bin/env bash
set -euo pipefail
export LC_ALL=C
# Tests the ERE patterns used by pusb_tmux_has_remote_clients() (src/tmux.c:190-192)
# to detect remote tmux clients in `w` output.
#
# At runtime the C code prepends the escaped username to each pattern via snprintf;
# we mirror that by prefixing "testuser" to the patterns here.
# Patterns are exercised with grep -E, which uses the same POSIX ERE engine.

pass=0
fail=0

# Patterns copied verbatim from src/tmux.c:191-192, prefixed with the test username.
TESTUSER="testuser"
PAT_IPV4="^${TESTUSER}([[:space:]]+)([^[:space:]]+)([[:space:]]+)([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})\.([0-9]{1,3})([[:space:]]+)(.+)tmux(.+)"
PAT_IPV6="^${TESTUSER}([[:space:]]+)([^[:space:]]+)([[:space:]]+)(([0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4})([[:space:]]+)(.+)tmux(.+)"

matches_pattern() {
    [[ "$2" =~ $1 ]]
}

assert_match() {
    local desc="$1" pattern="$2" line="$3"
    if matches_pattern "$pattern" "$line"; then
        printf '[PASS] %s\n' "$desc"
        pass=$((pass + 1))
    else
        printf '[FAIL] %s — expected match, got no match\n' "$desc"
        fail=$((fail + 1))
    fi
}

assert_no_match() {
    local desc="$1" pattern="$2" line="$3"
    if ! matches_pattern "$pattern" "$line"; then
        printf '[PASS] %s\n' "$desc"
        pass=$((pass + 1))
    else
        printf '[FAIL] %s — expected no match, got match\n' "$desc"
        fail=$((fail + 1))
    fi
}

# --- test fixtures ---

LINE_IPV4_TMUX="testuser pts/0    192.168.1.100    10:00    0.00s 0.01s 0.00s tmux: client"
LINE_IPV6_TMUX="testuser pts/1    fe80:0:0:1       10:00    0.00s 0.01s 0.00s tmux: client"
LINE_IPV6_LOOPBACK="testuser pts/2    ::1              10:00    0.00s 0.01s 0.00s tmux: client"
LINE_IPV6_LINKLOCAL="testuser pts/3    fe80::1          10:00    0.00s 0.01s 0.00s tmux: client"
LINE_IPV6_TYPICAL="testuser pts/4    2001:db8::1      10:00    0.00s 0.01s 0.00s tmux: client"
LINE_LOCAL_TMUX="testuser pts/2    -                10:00    0.00s 0.01s 0.00s tmux: client"
LINE_LOCAL_TMUX_HHMMSS="testuser pts/2    -                10:00:00 0.00s 0.01s 0.00s tmux: client"
LINE_IPV4_SSH="testuser pts/3    192.168.1.100    10:00    0.00s 0.01s 0.00s ssh"
LINE_X11_TMUX="testuser pts/4    :0               10:00    0.00s 0.01s 0.00s tmux: client"
LINE_OTHER_USER="otheruser pts/0   192.168.1.100    10:00    0.00s 0.01s 0.00s tmux: client"

# --- run tests ---

# IPv4 remote tmux → should match (remote client detected)
assert_match    "IPv4: remote IP + tmux → match"          "$PAT_IPV4" "$LINE_IPV4_TMUX"

# IPv6 remote tmux → should match (full and compressed forms)
assert_match    "IPv6: full 4-group + tmux → match"              "$PAT_IPV6" "$LINE_IPV6_TMUX"
assert_match    "IPv6: compressed ::1 (loopback) + tmux → match" "$PAT_IPV6" "$LINE_IPV6_LOOPBACK"
assert_match    "IPv6: compressed fe80::1 (link-local) + tmux → match" "$PAT_IPV6" "$LINE_IPV6_LINKLOCAL"
assert_match    "IPv6: compressed 2001:db8::1 + tmux → match"   "$PAT_IPV6" "$LINE_IPV6_TYPICAL"

# Local login (FROM='-') + tmux → no remote IP, should not match either pattern
assert_no_match "IPv4: local TTY (FROM=-) + tmux → no match"           "$PAT_IPV4" "$LINE_LOCAL_TMUX"
assert_no_match "IPv6: local TTY (FROM=-) + tmux → no match"           "$PAT_IPV6" "$LINE_LOCAL_TMUX"
assert_no_match "IPv6: local TTY HH:MM:SS idle time → no false match"  "$PAT_IPV6" "$LINE_LOCAL_TMUX_HHMMSS"

# IPv4 address but process is ssh, not tmux → should not match
assert_no_match "IPv4: remote IP + ssh (not tmux) → no match" "$PAT_IPV4" "$LINE_IPV4_SSH"

# X11 display (:0) + tmux → no valid IP, should not match either pattern
assert_no_match "IPv4: X11 :0 + tmux → no match"          "$PAT_IPV4" "$LINE_X11_TMUX"
assert_no_match "IPv6: X11 :0 + tmux → no match"          "$PAT_IPV6" "$LINE_X11_TMUX"

# Different username → pattern starts with "testuser", so a different user should not match
assert_no_match "IPv4: wrong username → no match"          "$PAT_IPV4" "$LINE_OTHER_USER"
# Username extension → "testuser2" must not match a pattern anchored for "testuser"
assert_no_match "IPv4: username extension (testuser2) → no match" "$PAT_IPV4" "testuser2 pts/0   192.168.1.100    10:00    0.00s 0.01s 0.00s tmux: client"
# Username suffix → "mytestuser" must not match a pattern for "testuser" (no ^ in old code)
assert_no_match "IPv4: username suffix (mytestuser) → no match"   "$PAT_IPV4" "mytestuser pts/0   192.168.1.100    10:00    0.00s 0.01s 0.00s tmux: client"
assert_no_match "IPv6: username suffix (mytestuser) → no match"   "$PAT_IPV6" "mytestuser pts/1    fe80::1          10:00    0.00s 0.01s 0.00s tmux: client"

# --- summary ---
echo "---"
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

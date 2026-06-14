#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

# Helper: run a [ ... ] test, check output, exit on failure
# $1 = description, $2 = test args, $3 = expected output
check() {
    out=$(../simpsh -c "[ $2 ] && echo y || echo n")
    if [ "$out" != "$3" ]; then
        msg_fail "[ $2 ] ($1): expected '$3' got '$out'"
        exit 1
    fi
}

msg_run 'fast path: [ 1 -eq 1 ]'
out=$(../simpsh -c '[ 1 -eq 1 ] && echo y || echo n')
[ "$out" = "y" ] || { msg_fail "fast path: got '$out'"; exit 1; }
test_pass "out" "matches" "y"

msg_run 'slow path: [ -f /etc/passwd -a -d /tmp ]'
out=$(../simpsh -c '[ -f /etc/passwd -a -d /tmp ] && echo y || echo n')
[ "$out" = "y" ] || { msg_fail "slow path: got '$out'"; exit 1; }
test_pass "out" "matches" "y"

msg_run '-z empty (unary string empty)'
empty=""
out=$(../simpsh -c "[ -z \"\$empty\" ] && echo y || echo n")
[ "$out" = "y" ] || { msg_fail "-z empty: got '$out'"; exit 1; }
test_pass "out" "matches" "y"

# === test -flag thing syntax (instead of [ ... ]) ===
msg_run 'test syntax: test -r /etc/passwd'
out=$(../simpsh -c 'test -r /etc/passwd && echo y || echo n')
[ "$out" = "y" ] || { msg_fail "test -r: got '$out'"; exit 1; }
test_pass "out" "matches" "y"

# === ! negation in unary: [ ! -f asdf ] ===
msg_run '! unary: [ ! -f nonexistent ]'
out=$(../simpsh -c '[ ! -f nonexistent ] && echo y || echo n')
[ "$out" = "y" ] || { msg_fail "! -f nonexistent: got '$out'"; exit 1; }
test_pass "out" "matches" "y"

# === ! negation in binary: [ ! 1 -eq 2 ] ===
msg_run '! binary: [ ! 1 -eq 2 ]'
out=$(../simpsh -c '[ ! 1 -eq 2 ] && echo y || echo n')
[ "$out" = "y" ] || { msg_fail "! 1 -eq 2: got '$out'"; exit 1; }
test_pass "out" "matches" "y"

suid_mode=$(stat -c '%a' /usr/bin/sudo 2>/dev/null | head -c1)
if [ "${suid_mode:-0}" = "4" ] || [ "${suid_mode:-0}" = "7" ]; then
    check "-u"  "-u /usr/bin/sudo" y
else
    check "-u"  "-u /usr/bin/sudo" n
fi

# === All flags loop: quiet on success, identifies which flag failed ===
msg_run 'all [ ] flags'
check "file exists"     "-e /etc/passwd"        y
check "regular file"    "-f /etc/passwd"        y
check "block dev"       "-b /dev/null"          n
check "char dev"        "-c /dev/null"          y
check "directory"       "-d /tmp"               y
check "fifo"            "-p /dev/null"          n
check "socket"          "-S /dev/null"          n
check "symlink"         "-h /dev/null"          n
check "readable"        "-r /etc/passwd"        y
check "writable"        "-w /tmp"               y
check "executable"      "-x /bin/sh"            y
check "size > 0"        "-s /etc/passwd"        y
check "string non-empty" "-n hello"             y
check "setuid"          "-u /usr/bin/sudo"      y
check "setgid"          "-g /usr/bin/sudo"      n
check "sticky"          "-k /tmp"               y
check "owned by me"     "-O $HOME"              y
check "group of me"     "-G $HOME"              y
check "="               "abc = abc"             y
check "!="              "abc != xyz"            y
check "int eq"          "1 -eq 1"               y
check "int ne"          "1 -ne 2"               y
check "int lt"          "1 -lt 2"               y
check "int gt"          "2 -gt 1"               y
check "int le"          "1 -le 1"               y
check "int ge"          "1 -ge 1"               y
check "file eq"         "/etc/passwd -ef /etc/passwd" y
# Don't add -nt/-ot as those depend on file mtimes
test_pass "all" "flags pass" "all"

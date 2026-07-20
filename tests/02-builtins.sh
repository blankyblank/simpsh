#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'echo test: "echo Test test!"'
out=$(../simpsh -c "echo Test test!")

if [ "$out" != "Test test!" ]; then
  test_fail "out" "differs from" "Test test!"
  exit 1
else
  test_pass "out" "matches" "Test test!"
fi

msg_run 'read builtin'
out=$(printf 'hello world\n' | ../simpsh -c 'read line; echo $line')
if [ "$out" = "hello world" ]; then
  test_pass "out" "matches" "hello world"
else
  test_fail "out" "expected" "hello world"
  exit 1
fi

msg_run 'printf builtin'
out=$(../simpsh -c 'printf "%d %s" 42 world')
if [ "$out" = "42 world" ]; then
  test_pass "out" "matches" "42 world"
else
  test_fail "out" "expected" "42 world"
  exit 1
fi

msg_run 'cd/pwd test: "cd /tmp ; pwd"'
out=$(../simpsh -c "cd /tmp ; pwd")

if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"
  exit 1
else
  test_pass "out" "matches" "/tmp"
fi

msg_run 'exit builtin test 1: "exit 0"; echo $?'
zero=$(../simpsh -c "exit 0"; echo $?)
if [ "$zero" -eq 0 ]; then
  msg_pass "\$zero=$zero correct exit status"
else
  msg_fail "exit 0 produced incorrect value"
  exit 1
fi

msg_run 'exit builtin test 2: exit 5" ; echo $?'
five=$(../simpsh -c "exit 5" ; echo $?)
if [ "$five" -eq 5 ]; then
  msg_pass "\$five=$five correct exit status"
else
  msg_fail "exit 5 produced incorrect value"
  exit 1
fi

msg_run 'eval'
out=$(../simpsh -c 'x=hello; eval "echo \$x"')
if [ "$out" = "hello" ]; then
  test_pass "out" "matches" "hello"
else
  test_fail "out" "expected" "hello"
  exit 1
fi

msg_run 'exec simple command'
out=$(../simpsh -c 'exec echo hi')
if [ "$out" = "hi" ]; then
  test_pass "out" "matches" "hi"
else
  test_fail "out" "expected" "hi"
  exit 1
fi

msg_run 'type builtin'
out=$(../simpsh -c 'type echo')
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected output" ""
  exit 1
fi

msg_run 'command -v'
out=$(../simpsh -c 'command -v echo')
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected output" ""
  exit 1
fi

# === kill ===
msg_run 'kill -l lists signal names'
out=$(../simpsh -c 'kill -l')
if [ -n "$out" ] && echo "$out" | grep -q "INT"; then
  test_pass "out" "contains INT" ""
else
  test_fail "out" "expected signal list" ""
  exit 1
fi

msg_run 'kill -0 checks process existence'
../simpsh -c 'kill -0 $$' 2>/dev/null
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "0 (process exists)" ""
else
  test_fail "rc" "expected 0" "$rc"
  exit 1
fi

# === hash ===
msg_run 'hash displays hash table'
out=$(../simpsh -c 'hash')
if [ $? -eq 0 ]; then
  test_pass "hash" "exits 0" ""
else
  test_fail "hash" "expected success" ""
  exit 1
fi

msg_run 'hash -r clears hash table'
../simpsh -c 'hash -r'
if [ $? -eq 0 ]; then
  test_pass "hash -r" "exits 0" ""
else
  test_fail "hash -r" "expected success" ""
  exit 1
fi

# === umask ===
msg_run 'umask prints current mask'
out=$(../simpsh -c 'umask')
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected umask" ""
  exit 1
fi

# === ulimit ===
msg_run 'ulimit -n prints fd limit'
out=$(../simpsh -c 'ulimit -n')
if [ "$out" -ge 0 ] 2>/dev/null; then
  test_pass "out" "valid number" ""
else
  test_fail "out" "expected numeric" "$out"
  exit 1
fi

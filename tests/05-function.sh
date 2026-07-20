#!/bin/sh

[ -f ./funcs ] && . ./funcs


msg_run "shell function unset test: f() { echo test123; }"
out=$(../simpsh -c "f() { echo test123; } ; f")
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi

msg_run "shell function unset test: f() { echo test123; }"
out1=$(../simpsh -c "f() { echo test123; }; unset -f f ; f" 2>&1)
if [ "$out1" != "../simpsh: f: command not found" ]; then
  test_fail "out1" "expected" "simpsh: f: command not found"
  exit 1
else
  test_pass "out1" "matches" "simpsh: f: command not found"
fi

msg_run 'local variable in function'
out=$(../simpsh -c 'f() { local x=42; echo $x; }; f; echo $x')
if [ "$out" = "42" ]; then
  test_pass "out" "matches" "42"
else
  test_fail "out" "expected" "42"
  exit 1
fi

msg_run 'return from function'
out=$(../simpsh -c 'f() { return 5; }; f; echo $?')
if [ "$out" = "5" ]; then
  test_pass "out" "matches" "5"
else
  test_fail "out" "expected" "5"
  exit 1
fi

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

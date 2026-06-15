#!/bin/sh

[ -f ./funcs ] && . ./funcs


msg_run "shell function test: (echo test123)"
out=$(../simpsh -c "f() { echo test123; } ; f")
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi


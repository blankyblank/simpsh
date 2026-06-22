#!/bin/sh

[ -f ./funcs ] && . ./funcs

../simpsh -c "{ echo true"
msg_run "brace test 1: { echo test123; }"
out=$(../simpsh -c "{ echo test123; }")
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi

msg_run "brace test 2: { true; false; } || echo test123"
out1=$(../simpsh -c "{ true; false; } || echo test123")
if [ "$out1" != "test123" ]; then
  test_fail "out1" "expected" "test123"
  exit 1
else
  test_pass "out1" "matches" "test123"
fi

msg_run "brace test 2: false && { echo test123; pwd; }"
out2=$(../simpsh -c "false && { echo test123; pwd; }")
if [ -z "$out2" ]; then
  test_pass "out2" "matches" ""
else
  test_fail "out2" "should be empty not" "test123"
  exit 1
fi

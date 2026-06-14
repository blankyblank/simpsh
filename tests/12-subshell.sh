#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run "running (echo test123)"
out=$(../simpsh -c "(echo test123)")
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi


msg_run "running (echo test123)"
out2=$(../simpsh -c "(echo -n a ; echo b)")
if [ "$out2" != "ab" ]; then
  test_fail "out2" "expected" "ab"
  exit 1
else
  test_pass  "out2" "matches" "ab"
fi

#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs


msg_run '"false && echo true || echo false"'
out1=$(../simpsh -c "false && echo true || echo false")
if [ "$out1" != false ]; then
  test_fail "out1" "expected" "false"
  exit 1
else
  test_pass  "out1" "matches" "false"
fi

msg_run '"true && echo true || echo false"'
out2=$(../simpsh -c "true && echo true || echo false")
if [ "$out2" != true ]; then
  test_fail "out2" "expected" "true"
  exit 1
else
  test_pass  "out2" "matches" "true"
fi

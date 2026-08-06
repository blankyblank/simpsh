#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'sleep 0.01 exits 0'
../simpsh -c 'sleep 0.01'
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "0 (slept)" ""
else
  test_fail "rc" "expected 0" "$rc"
  exit 1
fi

msg_run 'sleep invalid interval fails'
../simpsh -c 'sleep 5x' 2>/dev/null
rc=$?
if [ $rc -ne 0 ]; then
  test_pass "rc" "non-zero (invalid interval)" ""
else
  test_fail "rc" "expected non-zero" "$rc"
  exit 1
fi

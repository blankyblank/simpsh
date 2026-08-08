#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'sleep 0.01 exits 0'
../simpsh -c 'sleep 0.01'
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "was" "0"
else
  test_fail "rc" "expected" "0"
  exit 1
fi

msg_run 'sleep invalid interval fails'
../simpsh -c 'sleep 5x' 2>/dev/null
rc=$?
if [ $rc -ne 0 ]; then
  test_pass "rc" "was" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'sleep 0.3 (fractional)'
../simpsh -c 'sleep 0.3'
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "was" "0"
else
  test_fail "rc" "expected" "0"
  exit 1
fi

msg_run 'sleep 0.001m (fraction + minute suffix)'
../simpsh -c 'sleep 0.001m'
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "was" "0"
else
  test_fail "rc" "expected" "0"
  exit 1
fi

msg_run 'sleep 0.0001h (fraction + hour suffix)'
../simpsh -c 'sleep 0.0001h'
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "was" "0"
else
  test_fail "rc" "expected" "0"
  exit 1
fi

msg_run 'sleep 1e-3 (scientific notation rejected)'
../simpsh -c 'sleep 1e-3' 2>/dev/null
rc=$?
if [ $rc -ne 0 ]; then
  test_pass "rc" "was" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

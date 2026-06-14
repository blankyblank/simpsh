#!/bin/sh
# shellcheck disable=2116
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'if true; then echo true; fi'
out=$(../simpsh -c 'if true; then echo true; fi')
if [ "$out" != "true" ]; then
  test_fail "out" "expected" "true"
  exit 1
else
  test_pass "out" "matches" "true"
fi

msg_run 'if false; then echo true; else echo false; fi'
out1=$(../simpsh -c 'if false; then echo true; else echo false; fi')
if [ "$out1" != "false" ]; then
  test_fail "out1" "expected" "false"
  exit 1
else
  test_pass "out1" "matches" "false"
fi

msg_run 'if false; then echo a; elif true; echo true; else echo false; fi'
out2=$(../simpsh -c 'if false; then echo a; elif true; then echo true; else echo false; fi')
if [ "$out2" != "true" ]; then
  test_fail "out2" "expected" "true"
  exit 1
else
  test_pass "out2" "matches" "true"
fi


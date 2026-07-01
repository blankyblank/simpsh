#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run ' word=thisisatest; case "$word" in t*) echo "pass" ;; *) echo "fail" ;; esac '
out1=$(../simpsh -c ' word=thisisatest; case "$word" in t*) echo "pass" ;; *) echo "fail" ;; esac ')
if [ "$out1" = "pass" ]; then
  test_pass "out1" "matches" "pass"
else
  test_fail "out1" "expected" "pass"
  exit 1
fi

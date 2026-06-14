#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run '"testvar=1 env" | grep testvar'
out="$(../simpsh -c "testvar=1 env" | grep testvar)"

if [ "$out" = testvar=1 ]; then
  test_pass  "out" "matches" "testvar=1"
else
  test_fail  "out" "expected" "testvar=1"
  exit 1
fi

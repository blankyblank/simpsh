#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run '"testvar=1 env" | grep testvar'
out="$(../simpsh -c "testvar=1 env" | grep testvar)"

if [ "$out" = testvar=1 ]; then
  test_pass  "out" "matches" "testvar=1"
else
  msg_fail "\$out=$out differs from 'testvar=1'"
  exit 1
fi

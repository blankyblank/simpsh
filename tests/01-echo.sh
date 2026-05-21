#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run '"echo Test test!"'
out=$(../simpsh -c "echo Test test!")

if [ "$out" != "Test test!" ]; then
  msg_fail "output differs"
  exit 1
else
  test_pass "out" "matches" "Test test!"
fi

exit

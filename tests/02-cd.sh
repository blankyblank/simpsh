#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run '"cd /tmp ; pwd"'
out=$(../simpsh -c "cd /tmp ; pwd")

if [ "$out" != "/tmp" ]; then
  msg_fail "output differs"
  exit 1
else
  test_pass "out" "matches" "/tmp"
fi

exit 0

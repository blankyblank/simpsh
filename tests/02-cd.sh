#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run '"cd /tmp ; pwd"'
out=$(../simpsh -c "cd /tmp ; pwd")

if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"
  exit 1
else
  test_pass "out" "matches" "/tmp"
fi

exit 0

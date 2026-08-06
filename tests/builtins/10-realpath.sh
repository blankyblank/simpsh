#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

tmpr=/tmp/realpath.$$

msg_run 'realpath resolves to absolute path'
touch "$tmpr"
out=$(../simpsh -c "realpath $tmpr")
rm -f "$tmpr"
if [ "$out" = "$tmpr" ]; then
  test_pass "out" "matches" "$tmpr"
else
  test_fail "out" "expected" "$tmpr"
  exit 1
fi

msg_run 'realpath resolves .. components'
out=$(../simpsh -c 'realpath /tmp/../tmp')
if [ "$out" = "/tmp" ]; then
  test_pass "out" "matches" "/tmp"
else
  test_fail "out" "expected" "/tmp"
  exit 1
fi

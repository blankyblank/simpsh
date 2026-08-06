#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'head -n 2'
out=$(printf '1\n2\n3\n' | ../simpsh -c 'head -n 2')
if [ "$out" = "1
2" ]; then
  msg_pass "head -n 2"
else
  msg_fail "head -n 2: expected the first two lines '1','2'"
  exit 1
fi

msg_run 'head -n 1'
out=$(printf '1\n2\n3\n' | ../simpsh -c 'head -n 1')
if [ "$out" = "1" ]; then
  test_pass "out" "matches" "1"
else
  test_fail "out" "expected" "1"
  exit 1
fi

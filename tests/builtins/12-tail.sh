#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'tail -2'
out=$(printf '1\n2\n3\n4\n5\n' | ../simpsh -c 'tail -2')
if [ "$out" = "4
5" ]; then
  msg_pass "tail -2"
else
  msg_fail "tail -2: expected the last two lines '4','5'"
  exit 1
fi

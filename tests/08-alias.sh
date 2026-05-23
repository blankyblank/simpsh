#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
# shellcheck disable=2028

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run "\"alias ec='echo test123'\" \"ec\""
out1="$(printf '%s\n%s' "alias ec='echo test123'" "ec" | ../simpsh)"
if [ "$out1" != test123 ];then
  msg_fail "\$out1: $out1 differs from test123"
  exit 1
else
  test_pass  "out1" "matches" "test123"
fi

msg_run "alias ec='echo test123' 'unalias ec' 'ec'"
if printf '%s\n' "alias ec='echo test123'" "unalias ec" "ec" | ../simpsh 2>/dev/null; then
  msg_fail "command should have failed"
  exit 1
else
  msg_pass "aliases are being unset correctly"
fi

#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
# shellcheck disable=2028

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg "running printf %s\n alias ec='echo test123' 'unalias ec' 'ec' | ../simpsh..."
printf '%s\n' "alias ec='echo test123'" "unalias ec" "ec" | ../simpsh 2>/dev/null && { msg_fail "command should have failed" && exit 1;}

msg "running out1=$(printf '%s\n' "alias ec='echo test123'" "ec" | ../simpsh)..."
out1="$(printf '%s\n%s' "alias ec='echo test123'" "ec" | ../simpsh)"

echo $out1
msg "comparing results..."
if [ "$out1" != test123 ];then
  msg_fail "output differs"
  exit 1
fi

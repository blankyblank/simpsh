#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }


printf '%s\n' "alias ec='echo test123'" "unalias ec" "ec" | ../simpsh && { msg_fail "command should have failed" && exit 1;}

out1="$(printf '%s\n%s' "alias ec='echo test123'" "ec" | ../simpsh)"

if [ "$out1" != test123 ];then
  msg_fail "output differs"
  exit 1
fi

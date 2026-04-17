#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

out1=$(../simpsh -c "alias ec='echo test123'; ec")

if [ $out1 != test123 ];then
  msg_fail "output differs"
  exit 1
fi

../simpsh -c "alias ec='echo test123'; unalias ec; ec" && msg_fail "command should have failed" && exit 1

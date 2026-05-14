#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg 'running ../simpsh -c "export test1=1 ; env" | grep "test1=1"...'
out=$(../simpsh -c "export test1=1 ; env" | grep "test1=1")

if [ "$out" != "test1=1" ]; then
  msg_fail "output differs"
  exit 1
fi

exit 0

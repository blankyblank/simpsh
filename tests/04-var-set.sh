#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run 'export test1=1 ; env | grep "test1=1"'
out=$(../simpsh -c "export test1=1 ; env" | grep "test1=1")

if [ "$out" != "test1=1" ]; then
  msg_fail "\$out=$out differs from 'test1=1'"
  exit 1
else
  test_pass  "out" "matches" "test1=1"
fi

exit 0

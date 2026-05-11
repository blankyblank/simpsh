#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

_fail_stat=$(../simpsh -c 'false; echo $?')
_success_stat=$(../simpsh -c 'true; echo $?')

echo $_fail_stat
if [ "$_fail_stat" -eq 0 ]; then
  msg_fail "exit status shouldn't equal 0"
  exit 1
fi

if [ "$_success_stat" -ne 0 ]; then
  msg_fail "exit status should equal 0"
  exit 1
fi

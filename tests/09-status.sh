#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

_fail_stat=$(../simpsh -c "false; echo $?")
_success_stat=$(../simpsh -c "true; echo $?")

if [ "$_fail_stat" -eq 0 ]; then
  msg_fail "exit status shouldn't equal 0"
fi

if [ "$_success_stat" -ne 0 ]; then
  msg_fail "exit status should equal 0"
fi

#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

out=$(../simpsh -c "echo Test test!")

if [ "$out" != "Test test!" ]; then
  msg_fail "output differs"
  exit 1
fi

exit

#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

out=$(../simpsh -c "printf '%s' a; printf '%s' b; printf '%s' c")

if [ $out != abc ]; then
  msg_fail "one of the commands failed"
  exit 1
fi

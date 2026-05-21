#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run '"TMPDIR=/tmp cd; pwd"'
if ../simpsh -c "TMPDIR=/tmp cd; pwd" >/dev/null; then
  msg_pass "TMPDIR=/tmp cd"
else
  msg_fail "precmd builtin failed"; exit 1;
fi

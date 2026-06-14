#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run '"TMPDIR=/tmp cd; pwd"'
if ../simpsh -c "TMPDIR=/tmp cd; pwd" >/dev/null; then
  msg_pass "TMPDIR=/tmp cd"
else
  msg_fail "precmd builtin failed"; exit 1;
fi

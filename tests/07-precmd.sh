#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'precommand assignment test: "testvar=1 env" | grep testvar'
out="$(../simpsh -c "testvar=1 env" | grep testvar)"

if [ "$out" = testvar=1 ]; then
  test_pass  "out" "matches" "testvar=1"
else
  test_fail  "out" "expected" "testvar=1"
  exit 1
fi

msg_run 'builtin precommand assignment test: "TMPDIR=/tmp cd; pwd"'
if ../simpsh -c "TMPDIR=/tmp cd; pwd" >/dev/null; then
  msg_pass "TMPDIR=/tmp cd"
else
  msg_fail "precmd builtin failed"; exit 1;
fi

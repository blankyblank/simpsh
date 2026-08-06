#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'expand default tabstop 8'
out=$(printf 'a\tb\n' | ../simpsh -c 'expand')
if [ "$out" = "a       b" ]; then
  test_pass "out" "matches" "a + 7 spaces + b"
else
  test_fail "out" "expected" "a + 7 spaces + b"
  exit 1
fi

msg_run 'expand -t 4'
out=$(printf '\ta\n' | ../simpsh -c 'expand -t 4')
if [ "$out" = "    a" ]; then
  test_pass "out" "matches" "4 spaces + a"
else
  test_fail "out" "expected" "4 spaces + a"
  exit 1
fi

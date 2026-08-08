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

msg_run 'expand -i keeps tab after non-blank'
out=$(printf 'a\tb\n' | ../simpsh -c 'expand -i')
exp=$(printf 'a\tb')
if [ "$out" = "$exp" ]; then
  test_pass "out" "matches" "a<TAB>b"
else
  test_fail "out" "expected" "a<TAB>b"
  exit 1
fi

msg_run 'expand -i still expands leading tab'
out=$(printf '\ta\n' | ../simpsh -c 'expand -i')
if [ "$out" = "        a" ]; then
  test_pass "out" "matches" "8 spaces + a"
else
  test_fail "out" "expected" "8 spaces + a"
  exit 1
fi

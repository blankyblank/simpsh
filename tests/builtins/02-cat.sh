#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'cat reads stdin'
out=$(printf 'hello\n' | ../simpsh -c 'cat')
if [ "$out" = "hello" ]; then
  test_pass "out" "matches" "hello"
else
  test_fail "out" "expected" "hello"
  exit 1
fi

msg_run 'cat - reads stdin'
out=$(printf 'hi\n' | ../simpsh -c 'cat -')
if [ "$out" = "hi" ]; then
  test_pass "out" "matches" "hi"
else
  test_fail "out" "expected" "hi"
  exit 1
fi

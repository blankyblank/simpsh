#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'cut -d : -f 2'
out=$(printf 'a:b:c\n' | ../simpsh -c 'cut -d : -f 2')
if [ "$out" = "b" ]; then
  test_pass "out" "matches" "b"
else
  test_fail "out" "expected" "b"
  exit 1
fi

msg_run 'cut -c 2-4'
out=$(printf 'abcdef\n' | ../simpsh -c 'cut -c 2-4')
if [ "$out" = "bcd" ]; then
  test_pass "out" "matches" "bcd"
else
  test_fail "out" "expected" "bcd"
  exit 1
fi

msg_run 'cut -s suppresses lines without delimiter'
out=$(printf 'abc\n' | ../simpsh -c 'cut -s -d : -f 1')
if [ -z "$out" ]; then
  test_pass "out" "empty (line suppressed)" ""
else
  test_fail "out" "expected empty" "$out"
  exit 1
fi

#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'uniq removes adjacent duplicates'
out=$(printf 'a\na\nb\n' | ../simpsh -c 'uniq')
if [ "$out" = "a
b" ]; then
  msg_pass "uniq"
else
  msg_fail "uniq: duplicate 'a' must be collapsed"
  exit 1
fi

msg_run 'uniq -c counts occurrences'
out=$(printf 'a\na\nb\n' | ../simpsh -c 'uniq -c')
if [ "$out" = "2 a
1 b" ]; then
  msg_pass "uniq -c"
else
  msg_fail "uniq -c: expected '2 a' then '1 b'"
  exit 1
fi

msg_run 'uniq -u prints unique lines only'
out=$(printf 'a\na\nb\n' | ../simpsh -c 'uniq -u')
if [ "$out" = "b" ]; then
  test_pass "out" "matches" "b"
else
  test_fail "out" "expected" "b"
  exit 1
fi

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

msg_run 'uniq -d prints duplicates only'
out=$(printf 'a\na\nb\n' | ../simpsh -c 'uniq -d')
if [ "$out" = "a" ]; then
  test_pass "out" "matches" "a"
else
  test_fail "out" "expected" "a"
  exit 1
fi

msg_run 'uniq -f 1 (skip fields)'
out=$(printf 'a x\nb x\nc y\n' | ../simpsh -c 'uniq -f 1')
if [ "$out" = "a x
c y" ]; then
  msg_pass "uniq -f 1"
else
  msg_fail "uniq -f 1: 'a x' and 'b x' share field 2 and must collapse to 'a x'"
  exit 1
fi

msg_run 'uniq -s 2 (skip characters)'
out=$(printf 'aaa\naba\nbbb\n' | ../simpsh -c 'uniq -s 2')
if [ "$out" = "aaa
bbb" ]; then
  msg_pass "uniq -s 2"
else
  msg_fail "uniq -s 2: 'aaa' and 'aba' share the key after 2 chars and must collapse"
  exit 1
fi

#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'wc -l'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -l')
if [ "$out" = "   2" ]; then
  test_pass "out" "matches" "   2"
else
  test_fail "out" "expected" "   2"
  exit 1
fi

msg_run 'wc -w'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -w')
if [ "$out" = "   3" ]; then
  test_pass "out" "matches" "   3"
else
  test_fail "out" "expected" "   3"
  exit 1
fi

msg_run 'wc -c'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -c')
if [ "$out" = "   6" ]; then
  test_pass "out" "matches" "   6"
else
  test_fail "out" "expected" "   6"
  exit 1
fi

msg_run 'wc default (lines words bytes)'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc')
if [ "$out" = "   2    3    6" ]; then
  test_pass "out" "matches" "   2    3    6"
else
  test_fail "out" "expected" "   2    3    6"
  exit 1
fi

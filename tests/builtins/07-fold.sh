#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'fold -w 5'
out=$(printf '1234567890\n' | ../simpsh -c 'fold -w 5')
if [ "$out" = "12345
67890" ]; then
  msg_pass "fold -w 5"
else
  msg_fail "fold -w 5: expected '12345' then '67890'"
  exit 1
fi

msg_run 'fold -w 5 (no trailing newline)'
out=$(printf 'x'; printf '1234567890' | ../simpsh -c 'fold -w 5'; printf 'y')
if [ "$out" = "x12345
67890y" ]; then
  msg_pass "fold -w 5 no trailing newline"
else
  msg_fail "fold -w 5: the final partial line must not be dropped"
  exit 1
fi

msg_run 'fold -w 5 -s (break at spaces)'
out=$(printf 'aaa bb cc\n' | ../simpsh -c 'fold -w 5 -s')
if [ "$out" = "aaa 
bb cc" ]; then
  msg_pass "fold -w 5 -s"
else
  msg_fail "fold -w 5 -s: expected 'aaa ' then 'bb cc'"
  exit 1
fi

msg_run 'fold -w 5 -s (no trailing newline)'
out=$(printf 'x'; printf 'aaa bb cc' | ../simpsh -c 'fold -w 5 -s'; printf 'y')
if [ "$out" = "xaaa 
bb ccy" ]; then
  msg_pass "fold -w 5 -s no trailing newline"
else
  msg_fail "fold -w 5 -s: the final partial line must not be dropped"
  exit 1
fi

msg_run 'fold -b -w 4 (byte counting)'
out=$(printf 'x\ty\n' | ../simpsh -c 'fold -b -w 4')
exp=$(printf 'x\ty')
if [ "$out" = "$exp" ]; then
  msg_pass "fold -b -w 4"
else
  msg_fail "fold -b -w 4: tab must count as 1 column so 'x<TAB>y' fits in width 4"
  exit 1
fi

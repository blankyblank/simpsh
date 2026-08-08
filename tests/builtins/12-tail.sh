#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'tail -2'
out=$(printf '1\n2\n3\n4\n5\n' | ../simpsh -c 'tail -2')
if [ "$out" = "4
5" ]; then
  msg_pass "tail -2"
else
  msg_fail "tail -2: expected the last two lines '4','5'"
  exit 1
fi

msg_run 'tail -n 2'
out=$(printf '1\n2\n3\n4\n5\n' | ../simpsh -c 'tail -n 2')
if [ "$out" = "4
5" ]; then
  msg_pass "tail -n 2"
else
  msg_fail "tail -n 2: expected the last two lines '4','5'"
  exit 1
fi

msg_run 'tail -n 3 file'
tmpt=/tmp/tail.$$
printf '1\n2\n3\n4\n5\n' > "$tmpt"
out=$(../simpsh -c "tail -n 3 $tmpt")
rm -f "$tmpt"
if [ "$out" = "3
4
5" ]; then
  msg_pass "tail -n 3 file"
else
  msg_fail "tail -n 3: expected the last three lines '3','4','5'"
  exit 1
fi

msg_run 'tail -n 1 multiple files (headers)'
tmpt1=/tmp/tail1.$$
tmpt2=/tmp/tail2.$$
printf 'a\n' > "$tmpt1"
printf 'b\n' > "$tmpt2"
out=$(../simpsh -c "tail -n 1 $tmpt1 $tmpt2")
rm -f "$tmpt1" "$tmpt2"
exp="==> $tmpt1 <==
a

==> $tmpt2 <==
b"
if [ "$out" = "$exp" ]; then
  msg_pass "tail -n 1 multiple files"
else
  msg_fail "tail -n 1: expected '==> name <==' headers between file outputs"
  exit 1
fi

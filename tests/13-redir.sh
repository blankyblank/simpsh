#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

f="./redir.txt"
msg_run "echo test123 > $f ; cat $f"
out1=$(printf '%s\n' "echo test123 > $f" "cat $f" | ../simpsh)
if [ "$out1" != "test123" ]; then
  msg_fail "\$out1=$out1 differs from 'test123'"
  exit 1
else
  test_pass  "out1" "matches" "test123"
  # rm $f
fi


msg_run "echo hello >> $f ; cat $f"
../simpsh -c "echo hello >> $f"
out2=$(cat ./redir.txt)
res="test123
hello"
if [ "$out2" != "$res" ]; then
  msg_fail "\$out2=$out2 differs from 'ab'"
  exit 1
else
  msg_pass ">> append redirection"
  rm $f
fi

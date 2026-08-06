#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

tmpt=/tmp/tee.$$

msg_run 'tee writes to stdout and file'
out=$(printf 'hi\n' | ../simpsh -c "tee $tmpt")
fout=$(cat "$tmpt")
rm -f "$tmpt"
if [ "$out" = "hi" ] && [ "$fout" = "hi" ]; then
  test_pass "out" "matches" "hi"
else
  test_fail "out" "expected 'hi' in stdout and file" ""
  exit 1
fi

msg_run 'tee -a appends'
printf 'a\n' > "$tmpt"
out=$(printf 'b\n' | ../simpsh -c "tee -a $tmpt")
fout=$(cat "$tmpt")
rm -f "$tmpt"
if [ "$out" = "b" ] && [ "$fout" = "a
b" ]; then
  test_pass "out" "matches" "a,b in file"
else
  test_fail "out" "expected 'b' to stdout and 'a','b' in file" ""
  exit 1
fi

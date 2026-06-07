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

echo "input_test" > $f
msg_run "cat < $f"
out=$(../simpsh -c "cat < $f")
[ "$out" = "input_test" ] && msg_pass "input redirection" || { msg_fail "input redirection: $out"; exit 1; }
rm $f

echo "original" > $f
msg_run "set -C; echo new > $f (should fail)"
../simpsh -c "set -C; echo new > $f" 2>/dev/null
out=$(cat $f)
[ "$out" = "original" ] && msg_pass "Cflag blocks overwrite" || msg_fail "Cflag"
msg_run "set -C; echo new >| $f (should work)"
../simpsh -c "set -C; echo new >| $f"
out=$(cat $f)
[ "$out" = "new" ] && msg_pass "noclobber" || { msg_fail "noclobber"; exit 1; }
rm $f

msg_run "echo stderr >&2"
out=$(../simpsh -c "echo hello 2>&1 1>/dev/null")
[ -z "$out" ] && msg_pass "redirect duplication" || { msg_fail "redirect duplication"; exit 1; }

msg_run "cat << EOF"
out=$(../simpsh -c 'cat << EOF
hello world
EOF')
[ "$out" = "hello world" ] && msg_pass "heredoc" || { msg_fail "heredoc: $out"; exit 1; }

out=$(env tmpvar=expanded ../simpsh -c 'cat << '\''EOF'\''
$tmpvar
EOF')
[ "$out" = '$tmpvar' ] && msg_pass "quoted heredoc" || { msg_fail "quoted heredoc: $out"; exit 1; }

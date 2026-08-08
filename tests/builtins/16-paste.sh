#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

tmpp1=/tmp/pastep1.$$
tmpp2=/tmp/pastep2.$$
tmpp3=/tmp/pastep3.$$

msg_run 'paste parallel (basic)'
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'a\tx\nb\ty')
if [ "$out" = "$exp" ]; then
  msg_pass "paste parallel (basic)"
else
  msg_fail "paste: expected TAB-joined rows"
  exit 1
fi

msg_run 'paste first file longer'
printf 'a\nb\nc\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'a\tx\nb\ty\nc\t')
if [ "$out" = "$exp" ]; then
  msg_pass "paste first file longer"
else
  msg_fail "paste: expected trailing TAB on last rows"
  exit 1
fi

msg_run 'paste second file longer'
printf 'a\n' > "$tmpp1"
printf 'x\ny\nz\n' > "$tmpp2"
out=$(../simpsh -c "paste $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'a\tx\n\ty\n\tz')
if [ "$out" = "$exp" ]; then
  msg_pass "paste second file longer"
else
  msg_fail "paste: expected leading TAB on continued rows"
  exit 1
fi

msg_run 'paste empty file in middle'
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
: > "$tmpp3"
out=$(../simpsh -c "paste $tmpp1 $tmpp3 $tmpp2")
rm -f "$tmpp1" "$tmpp2" "$tmpp3"
exp=$(printf 'a\t\tx\nb\t\ty')
if [ "$out" = "$exp" ]; then
  msg_pass "paste empty file in middle"
else
  msg_fail "paste: expected empty field for dead file"
  exit 1
fi

msg_run 'paste -d \0: parallel'
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -d '\0:' $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'ax\nby')
if [ "$out" = "$exp" ]; then
  msg_pass "paste -d \0: parallel"
else
  msg_fail "paste -d: expected \0 (empty) delimiter each row"
  exit 1
fi

msg_run 'paste -d list cycles per row'
printf 'a\nb\nc\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -d ':;' $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'a:x\nb:y\nc:')
if [ "$out" = "$exp" ]; then
  msg_pass "paste -d list cycles per row"
else
  msg_fail "paste -d: expected delim list to restart each row"
  exit 1
fi

msg_run 'paste -d "" (no delimiters)'
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -d '' $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'ax\nby')
if [ "$out" = "$exp" ]; then
  msg_pass "paste -d \"\" (no delimiters)"
else
  msg_fail "paste -d: expected no delimiters emitted"
  exit 1
fi

msg_run 'paste -s serial'
printf 'a\nb\nc\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -s $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'a\tb\tc\nx\ty')
if [ "$out" = "$exp" ]; then
  msg_pass "paste -s serial"
else
  msg_fail "paste -s: expected one output line per file"
  exit 1
fi

msg_run 'paste -s empty file in middle'
printf 'a\nb\nc\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
: > "$tmpp3"
out=$(../simpsh -c "paste -s $tmpp1 $tmpp3 $tmpp2")
rm -f "$tmpp1" "$tmpp2" "$tmpp3"
exp=$(printf 'a\tb\tc\n\nx\ty')
if [ "$out" = "$exp" ]; then
  msg_pass "paste -s empty file in middle"
else
  msg_fail "paste -s: expected blank line for empty file"
  exit 1
fi

msg_run 'paste -s -d \0: (per-file cycle reset)'
printf 'a\nb\nc\n' > "$tmpp1"
out=$(../simpsh -c "paste -s -d '\0:' $tmpp1")
rm -f "$tmpp1"
if [ "$out" = "ab:c" ]; then
  test_pass "out" "matches" "ab:c"
else
  test_fail "out" "expected" "ab:c"
  exit 1
fi

msg_run 'paste no operands (stdin passthrough)'
out=$(printf 'a\nb\n' | ../simpsh -c 'paste')
exp=$(printf 'a\nb')
if [ "$out" = "$exp" ]; then
  msg_pass "paste no operands (stdin passthrough)"
else
  msg_fail "paste: expected stdin copied to stdout"
  exit 1
fi

msg_run 'paste - - reads stdin once'
out=$(printf 'x\n' | ../simpsh -c 'paste - -')
exp=$(printf 'x\t')
if [ "$out" = "$exp" ]; then
  msg_pass "paste - - reads stdin once"
else
  msg_fail "paste - -: expected 'x' with trailing TAB"
  exit 1
fi

msg_run 'paste empty file alone'
: > "$tmpp3"
out=$(../simpsh -c "paste $tmpp3")
rm -f "$tmpp3"
if [ -z "$out" ]; then
  test_pass "out" "empty (empty file)" ""
else
  test_fail "out" "expected empty" ""
  exit 1
fi

msg_run 'paste -s empty file alone (blank line)'
: > "$tmpp3"
out=$(../simpsh -c "paste -s $tmpp3" | sed -n l)
rm -f "$tmpp3"
if [ "$out" = '$' ]; then
  test_pass "out" "single blank line" '$'
else
  test_fail "out" "expected blank line" '$'
  exit 1
fi

msg_run 'paste missing file'
out=$(../simpsh -c 'paste /nonexistent' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'paste -d trailing backslash'
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -d '\\' $tmpp1 $tmpp2" 2>&1)
rc=$?
rm -f "$tmpp1" "$tmpp2"
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'paste last -d wins'
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -d ':' -d ';' $tmpp1 $tmpp2")
rm -f "$tmpp1" "$tmpp2"
exp=$(printf 'a;x\nb;y')
if [ "$out" = "$exp" ]; then
  msg_pass "paste last -d wins"
else
  msg_fail "paste: expected last -d list to take effect"
  exit 1
fi

msg_run 'paste -d list too long'
d=''
i=0
while [ $i -lt 300 ]; do
  d="${d}a"
  i=$((i + 1))
done
printf 'a\nb\n' > "$tmpp1"
printf 'x\ny\n' > "$tmpp2"
out=$(../simpsh -c "paste -d '$d' $tmpp1 $tmpp2" 2>&1)
rc=$?
rm -f "$tmpp1" "$tmpp2"
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

tmpc1=/tmp/commc1.$$
tmpc2=/tmp/commc2.$$

msg_run 'comm default columns'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
exp=$(printf 'a\n\t\tb\n\tc')
if [ "$out" = "$exp" ]; then
  msg_pass "comm default columns"
else
  msg_fail "comm: expected 'a', common 'b' in col 3, 'c' in col 2"
  exit 1
fi

msg_run 'comm -12 shows common lines only'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -12 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
if [ "$out" = "b" ]; then
  test_pass "out" "matches" "b"
else
  test_fail "out" "expected" "b"
  exit 1
fi

msg_run 'comm -1 (suppress file 1 column)'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -1 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
exp=$(printf '\tb\nc')
if [ "$out" = "$exp" ]; then
  msg_pass "comm -1"
else
  msg_fail "comm -1: expected common 'b' (one tab) and file 2 only 'c'"
  exit 1
fi

msg_run 'comm -2 (suppress file 2 column)'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -2 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
exp=$(printf 'a\n\tb')
if [ "$out" = "$exp" ]; then
  msg_pass "comm -2"
else
  msg_fail "comm -2: expected file 1 only 'a' and common 'b' (one tab)"
  exit 1
fi

msg_run 'comm -3 (suppress common column)'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -3 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
exp=$(printf 'a\n\tc')
if [ "$out" = "$exp" ]; then
  msg_pass "comm -3"
else
  msg_fail "comm -3: expected file 1 only 'a' and file 2 only 'c' (one tab)"
  exit 1
fi

msg_run 'comm -13 (file 2 only)'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -13 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
if [ "$out" = "c" ]; then
  test_pass "out" "matches" "c"
else
  test_fail "out" "expected" "c"
  exit 1
fi

msg_run 'comm -23 (file 1 only)'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -23 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
if [ "$out" = "a" ]; then
  test_pass "out" "matches" "a"
else
  test_fail "out" "expected" "a"
  exit 1
fi

msg_run 'comm -123 (all columns suppressed)'
printf 'a\nb\n' > "$tmpc1"
printf 'b\nc\n' > "$tmpc2"
out=$(../simpsh -c "comm -123 $tmpc1 $tmpc2")
rm -f "$tmpc1" "$tmpc2"
if [ -z "$out" ]; then
  test_pass "out" "empty (all columns suppressed)" ""
else
  test_fail "out" "expected empty" ""
  exit 1
fi

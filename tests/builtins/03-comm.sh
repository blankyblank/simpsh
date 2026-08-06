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

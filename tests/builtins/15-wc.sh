#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'wc -l'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -l')
if [ "$out" = "2" ]; then
  test_pass "out" "matches" "2"
else
  test_fail "out" "expected" "2"
  exit 1
fi

msg_run 'wc -w'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -w')
if [ "$out" = "3" ]; then
  test_pass "out" "matches" "3"
else
  test_fail "out" "expected" "3"
  exit 1
fi

msg_run 'wc -c'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -c')
if [ "$out" = "6" ]; then
  test_pass "out" "matches" "6"
else
  test_fail "out" "expected" "6"
  exit 1
fi

msg_run 'wc default (lines words bytes)'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc')
if [ "$out" = "      2       3       6" ]; then
  test_pass "out" "matches" "   2    3    6"
else
  test_fail "out" "expected" "      2       3       6"
  exit 1
fi

msg_run 'wc -lw (flag combo)'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -lw')
if [ "$out" = "      2       3" ]; then
  test_pass "out" "matches" "      2       3"
else
  test_fail "out" "expected" "      2       3"
  exit 1
fi

msg_run 'wc -cl (flag combo)'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -cl')
if [ "$out" = "      2       6" ]; then
  test_pass "out" "matches" "      2       6"
else
  test_fail "out" "expected" "      2       6"
  exit 1
fi

msg_run 'wc -cw (flag combo)'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -cw')
if [ "$out" = "       3       6" ]; then
  test_pass "out" "matches" "       3       6"
else
  test_fail "out" "expected" "       3       6"
  exit 1
fi

msg_run 'wc -lwc (flag combo)'
out=$(printf 'a b\nc\n' | ../simpsh -c 'wc -lwc')
if [ "$out" = "      2       3       6" ]; then
  test_pass "out" "matches" "      2       3       6"
else
  test_fail "out" "expected" "      2       3       6"
  exit 1
fi

msg_run 'wc empty input'
out=$(printf '' | ../simpsh -c 'wc')
if [ "$out" = "      0       0       0" ]; then
  test_pass "out" "matches" "      0       0       0"
else
  test_fail "out" "expected" "      0       0       0"
  exit 1
fi

msg_run 'wc no trailing newline'
out=$(printf 'a b' | ../simpsh -c 'wc')
if [ "$out" = "      0       2       3" ]; then
  test_pass "out" "matches" "      0       2       3"
else
  test_fail "out" "expected" "      0       2       3"
  exit 1
fi

msg_run 'wc single file'
out=$(../simpsh -c 'wc /dev/null')
if [ "$out" = "0 0 0 /dev/null" ]; then
  test_pass "out" "matches" "0 0 0 /dev/null"
else
  test_fail "out" "expected" "0 0 0 /dev/null"
  exit 1
fi

msg_run 'wc multiple files totals'
tmpw1=/tmp/wc1.$$
tmpw2=/tmp/wc2.$$
printf 'a\n' > "$tmpw1"
printf 'b\n' > "$tmpw2"
out=$(../simpsh -c "wc $tmpw1 $tmpw2")
rm -f "$tmpw1" "$tmpw2"
exp="1 1 2 $tmpw1
1 1 2 $tmpw2
2 2 4 total"
if [ "$out" = "$exp" ]; then
  msg_pass "wc multiple files totals"
else
  msg_fail "wc multiple files: expected per-file counts and a 'total' row"
  exit 1
fi

msg_run 'wc missing file'
out=$(../simpsh -c 'wc /nonexistent' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ $rc -eq 1 ]; then
  test_pass "out" "empty (missing file)" ""
else
  test_fail "out" "expected empty stdout" ""
  exit 1
fi

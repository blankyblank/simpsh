#!/bin/sh

# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

out=$(../simpsh -c 'i=0; i=$((i + 1)); echo $i')
[ "$out" = 1 ] || exit 1
msg_run 'arithmetic test 1:'
../simpsh -c 'echo $((1 + 2 * 3))'
out=$(../simpsh -c 'echo $((1 + 2 * 3))')
if [ "$out" = 7 ]; then
  test_pass "out" "matches" "7"
else
  test_fail "out" "expected" "7"
  exit 1
fi

msg_run 'arithmetic basic: echo $((1 + 2 * 3))'
out=$(../simpsh -c 'echo $((1 + 2 * 3))')
[ "$out" = 7 ] || {
  msg_fail "basic arithemetic"
  exit 1
}

msg_run 'arithmetic bitwise: echo $((2 & 3))'
out=$(../simpsh -c 'echo $((2 & 3))')
[ "$out" = 2 ] || {
  msg_failed "& "
  exit 1
}

msg_run 'arithmetic bitwise: echo $((2 | 4))'
out=$(../simpsh -c 'echo $((2 | 4))')
[ "$out" = 6 ] || {
  msg_fail "| "
  exit 1
}

msg_run 'arithmetic bitwise: echo $((2 ^ 3))'
out=$(../simpsh -c 'echo $((2 ^ 3))')
[ "$out" = 1 ] || {
  msg_fail "^ "
  exit 1
}

msg_run 'arithmetic bitwise: echo $((1 << 3))'
out=$(../simpsh -c 'echo $((1 << 3))')
[ "$out" = 8 ] || {
  msg_fail "<< "
  exit 1
}

msg_run 'arithmetic bitwise: echo $((16 >> 2))'
out=$(../simpsh -c 'echo $((16 >> 2))')
[ "$out" = 4 ] || {
  msg_fail ">> "
  exit 1
}

msg_run 'arithmetic logical: echo $((!0))'
out=$(../simpsh -c 'echo $((!0))')
[ "$out" = 1 ] || {
  msg_fail "! "
  exit 1
}

msg_run 'arithmetic logical: echo $((0 || 1))'
out=$(../simpsh -c 'echo $((0 || 1))')
[ "$out" = 1 ] || { msg_fail "; " || exit 1; }

msg_run 'arithmetic logical: echo $((1 && 0))'
out=$(../simpsh -c 'echo $((1 && 0))')
[ "$out" = 0 ] || {
  msg_fail "&& "
  exit 1
}

msg_run 'arithmetic modulo: echo $((7 % 3))'
out=$(../simpsh -c 'echo $((7 % 3))')
[ "$out" = 1 ] || {
  msg_fail "% "
  exit 1
}

msg_run 'arithmetic nested parens: echo $(((1 + 2) * 3))'
out=$(../simpsh -c 'echo $(((1 + 2) * 3))')
[ "$out" = 9 ] || {
  msg_fail "(1 + 2) * 3 "
  exit 1
}

msg_run 'arithmetic comparison: echo $((3 > 2))'
out=$(../simpsh -c 'echo $((3 > 2))')
[ "$out" = 1 ] || {
  msg_fail ">"
  exit 1
}

msg_run 'arithmetic comparison: echo $((3 < 2))'
out=$(../simpsh -c 'echo $((3 < 2))')
[ "$out" = 0 ] || {
  msg_fail "< "
  exit 1
}

msg_run 'arithmetic hex: echo $((0xff))'
out=$(../simpsh -c 'echo $((0xff))')
[ "$out" = 255 ] || {
  msg_fail "hex "
  exit 1
}

msg_run 'arithmetic octal: echo $((077))'
out=$(../simpsh -c 'echo $((077))')
[ "$out" = 63 ] || {
  msg_fail "octal "
  exit 1
}

msg_run 'arithmetic negative: echo $((-5 + 3))'
out=$(../simpsh -c 'echo $((-5 + 3))')
[ "$out" = "-2" ] || {
  echo $out
  msg_fail "negative arith "
  exit 1
}

msg_run 'arithmetic double negative: echo $((- -5))'
out=$(../simpsh -c 'echo $((- -5))')
[ "$out" = 5 ] || {
  msg_fail "double negative "
  exit 1
}

msg_run 'arithmetic assignment: x=5; echo $((x = x + 2)); echo $x'
out=$(../simpsh -c 'x=5; y=$((x = x + 2)); echo $y')
[ "$out" = "7" ] || { msg_fail "arith assignment" ; exit 1; }

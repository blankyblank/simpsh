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

msg_run 'compound add: x=5; echo $((x += 3)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x += 3)); echo $x')
[ "$out" = "8
8" ] || { msg_fail "x += 3"; exit 1; }

msg_run 'compound subtract: x=5; echo $((x -= 2)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x -= 2)); echo $x')
[ "$out" = "3
3" ] || { msg_fail "x -= 2"; exit 1; }

msg_run 'compound multiply: x=2; echo $((x *= 5)); echo $x'
out=$(../simpsh -c 'x=2; echo $((x *= 5)); echo $x')
[ "$out" = "10
10" ] || { msg_fail "x *= 10"; exit 1; }

msg_run 'compound divide: x=10; echo $((x /= 4)); echo $x'
out=$(../simpsh -c 'x=10; echo $((x /= 4)); echo $x')
[ "$out" = "2
2" ] || { msg_fail "x /= 4"; exit 1; }

msg_run 'compound modulo: x=10; echo $((x %= 4)); echo $x'
out=$(../simpsh -c 'x=10; echo $((x %= 4)); echo $x')
[ "$out" = "2
2" ] || { msg_fail "x %= 4"; exit 1; }

msg_run 'compound bitwise: x=1; echo $((x <<= 2)); echo $x'
out=$(../simpsh -c 'x=1; echo $((x <<= 2)); echo $x')
[ "$out" = "4
4" ] || { msg_fail "x <<= 2"; exit 1; }

msg_run 'compound bitwise: x=8; echo $((x >>= 2)); echo $x'
out=$(../simpsh -c 'x=8; echo $((x >>= 2)); echo $x')
[ "$out" = "2
2" ] || { msg_fail "x >>= 2"; exit 1; }

msg_run 'compound and: x=5; echo $((x &= 3)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x &= 3)); echo $x')
[ "$out" = "1
1" ] || { msg_fail "x &= 3"; exit 1; }

# came out to 15 in dash when testing
msg_run 'compound or: x=5; echo $((x |= 8)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x |= 8)); echo $x')
[ "$out" = "13
13" ] || { msg_fail "x |= 8"; exit 1; }

msg_run 'compound xor: x=5; echo $((x ^= 3)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x ^= 3)); echo $x')
[ "$out" = "6
6" ] || { msg_fail "x ^= 3"; exit 1; }

msg_run 'while: : $((retval += $?)) after false'
out=$(../simpsh -c 'retval=0; false; : $((retval += $?)); echo $retval')
[ "$out" = 1 ] || { msg_fail "retval += \$?"; exit 1; }

msg_run 'compound across statements: : $((x += 1)) twice'
out=$(../simpsh -c 'x=1; : $((x += 1)); : $((x += 1)); echo $x')
[ "$out" = 3 ] || { msg_fail "x += 1 twice"; exit 1; }

msg_run 'post increment: x=5; echo $((x++)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x++)); echo $x')
[ "$out" = "5
6" ] || { msg_fail "x++"; exit 1; }

msg_run 'preincrement: x=5; echo $((++x)); echo $x'
out=$(../simpsh -c 'x=5; echo $((++x)); echo $x')
[ "$out" = "6
6" ] || { msg_fail "++x"; exit 1; }

msg_run 'post decrement: x=5; echo $((x--)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x--)); echo $x')
[ "$out" = "5
4" ] || { msg_fail "x--"; exit 1; }

msg_run 'post increment mixed: x=5; echo $((x++ + x)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x++ + x)); echo $x')
[ "$out" = "11
6" ] || { msg_fail "x++ + x"; exit 1; }

msg_run 'post decrement mixed: x=5; echo $((x-- * x)); echo $x'
out=$(../simpsh -c 'x=5; echo $((x-- * x)); echo $x')
[ "$out" = "20
4" ] || { msg_fail "x-- * x"; exit 1; }

msg_run 'post decrement null variable: x; echo $((x++)); echo $x'
out=$(../simpsh -c 'x; echo $((x++)); echo $x')
[ "$out" = "0
1" ] || { msg_fail "x++"; exit 1; }

msg_run 'tokenize tests: echo $((5--2))'
out=$(../simpsh -c 'echo $((5--2))')
[ "$out" = "7" ] || { msg_fail "5--2"; exit 1; }

msg_run 'echo $((--5))'
out=$(../simpsh -c 'echo $((--5))')
[ "$out" = "5" ] || { msg_fail "--5"; exit 1; }

msg_run 'x=5; echo $((x + 2 + 3))'
out=$(../simpsh -c 'x=5; echo $((x + 2 + 3))')
[ "$out" = "10" ] || { msg_fail "x + 2 + 3"; exit 1; }

msg_run 'x=5; echo $((x - 1 - 1))'
out=$(../simpsh -c 'x=5; echo $((x - 1 - 1))')
[ "$out" = "3" ] || { msg_fail "x - 1 - 1"; exit 1; }

msg_run 'x=5; echo $((x + 2))'
out=$(../simpsh -c 'x=5; echo $((x + 2))')
[ "$out" = "7" ] || { msg_fail "x + 2"; exit 1; }

msg_run 'a=5 b=3; echo $((a + b + b))'
out=$(../simpsh -c 'a=5 b=3; echo $((a + b + b))')
[ "$out" = "11" ] || { msg_fail "a + b + b"; exit 1; }

msg_run "arithmetic vars: a=5 b=3; echo \$((a + b))"
out=$(../simpsh -c 'a=5 b=3; echo $((a + b))')
if [ "$out" != "8" ]; then
  test_fail "out" "expected" "8"; exit 1
else
  test_pass "out" "matches" "8"
fi

msg_run "arithmetic assignment: x=5; echo \$((x = x + 2)); echo \$x"
out=$(../simpsh -c 'x=5; echo $((x = x + 2)); echo $x')
if [ "$out" != "7
7" ]; then
  test_fail "out" "expected" "7 7"; exit 1
else
  test_pass "out" "matches" "7 7"
fi

msg_run 'quoted arith: echo "$((1 + 2))"'
out=$(../simpsh -c 'echo "$((1 + 2))"')
if [ "$out" != "3" ]; then
  test_fail "out" "expected" "3"; exit 1
else
  test_pass "out" "matches" "3"
fi

msg_run 'nested arith: echo $(($((1 + 2)) * 2))'
out=$(../simpsh -c 'echo $(($((1 + 2)) * 2))')
if [ "$out" != "6" ]; then
  test_fail "out" "expected" "6"; exit 1
else
  test_pass "out" "matches" "6"
fi

msg_run 'arith equality: echo $((1 == 1)) $((1 != 2))'
out=$(../simpsh -c 'echo $((1 == 1)) $((1 != 2))')
if [ "$out" != "1 1" ]; then
  test_fail "out" "expected" "1 1"; exit 1
else
  test_pass "out" "matches" "1 1"
fi

msg_run 'ternary true: echo $((1 ? 10 : 20))'
out=$(../simpsh -c 'echo $((1 ? 10 : 20))')
[ "$out" = "10" ] || {
  msg_fail "?: true"
  exit 1
}

msg_run 'ternary false: echo $((0 ? 10 : 20))'
out=$(../simpsh -c 'echo $((0 ? 10 : 20))')
[ "$out" = "20" ] || {
  msg_fail "?: false"
  exit 1
}

msg_run 'ternary nested right-assoc: echo $((1 ? 0 ? 1 : 2 : 3))'
out=$(../simpsh -c 'echo $((1 ? 0 ? 1 : 2 : 3))')
[ "$out" = "2" ] || {
  msg_fail "?: nested"
  exit 1
}

msg_run 'ternary with vars (shellbench shape): ex=2; echo $(($ex == 0 ? 1 : $ex))'
out=$(../simpsh -c 'ex=2; echo $(($ex == 0 ? 1 : $ex))')
[ "$out" = "2" ] || {
  msg_fail "?: shellbench shape"
  exit 1
}

msg_run 'ternary condition from comparison: echo $((3 > 2 ? 7 : 8))'
out=$(../simpsh -c 'echo $((3 > 2 ? 7 : 8))')
[ "$out" = "7" ] || {
  msg_fail "?: comparison cond"
  exit 1
}

msg_run 'div by zero aborts rc=2, no output'
out=$(../simpsh -c 'echo $((1/0))' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ "$rc" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "out/rc" "unexpected" "$out/$rc"
  exit 1
fi

msg_run 'assign div by zero aborts rc=2'
../simpsh -c 'x=$((1/0))' 2>/dev/null
if [ "$?" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'mod by zero aborts rc=2'
../simpsh -c 'echo $((1%0))' 2>/dev/null
if [ "$?" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'div by zero in if condition aborts, no branch'
out=$(../simpsh -c 'if [ $((1/0)) -eq 0 ]; then echo y; else echo n; fi' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ "$rc" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "out/rc" "unexpected" "$out/$rc"
  exit 1
fi

msg_run 'div by zero in for words aborts'
out=$(../simpsh -c 'for i in $((1/0)); do echo $i; done' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ "$rc" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "out/rc" "unexpected" "$out/$rc"
  exit 1
fi

msg_run 'div by zero in case word aborts'
out=$(../simpsh -c 'case $((1/0)) in 0) echo y;; *) echo n;; esac' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ "$rc" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "out/rc" "unexpected" "$out/$rc"
  exit 1
fi

msg_run 'div by zero in && RHS aborts'
out=$(../simpsh -c 'true && echo $((1/0))' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ "$rc" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "out/rc" "unexpected" "$out/$rc"
  exit 1
fi

msg_run 'valid division unaffected'
out=$(../simpsh -c 'echo $((7/2)) $((7%3))')
if [ "$out" = "3 1" ]; then test_pass "out" "matches" "3 1"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'div by zero in redirect target aborts rc=2, no file'
rm -f ./testfiles/dz_*
../simpsh -c 'echo hi > ./testfiles/dz_$((1/0))' 2>/dev/null
rc=$?
if [ "$rc" = "2" ] && [ -z "$(ls ./testfiles/dz_* 2>/dev/null)" ]; then test_pass "rc" "matches, no file" "2"; else
  test_fail "rc/out" "unexpected" "$rc"
  rm -f ./testfiles/dz_*
  exit 1
fi
rm -f ./testfiles/dz_*

msg_run 'div by zero in heredoc body aborts rc=2 (dash silently continues; we fail loud)'
out=$(../simpsh -c 'read v <<EOF
$((1/0))
EOF
echo after' 2>/dev/null)
rc=$?
if [ -z "$out" ] && [ "$rc" = "2" ]; then test_pass "rc" "matches" "2"; else
  test_fail "out/rc" "unexpected" "$out/$rc"
  exit 1
fi

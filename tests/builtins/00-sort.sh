#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

tmpout=/tmp/sortout.$$
tmpm1=/tmp/sortm1.$$
tmpm2=/tmp/sortm2.$$

msg_run 'sort -k 2'
out=$(printf 'b 2\na 1\n' | ../simpsh -c 'sort -k 2')
if [ "$out" = "a 1
b 2" ]; then
  msg_pass "sort -k 2"
else
  msg_fail "sort -k 2: expected 'a 1' before 'b 2' (byte order of field 2)"
  exit 1
fi

msg_run 'sort -k 2n (numeric)'
out=$(printf 'b 10\na 9\n' | ../simpsh -c 'sort -k 2n')
if [ "$out" = "a 9
b 10" ]; then
  msg_pass "sort -k 2n"
else
  msg_fail "sort -k 2n: expected 'a 9' before 'b 10' (numeric compare)"
  exit 1
fi

msg_run 'sort -k 2 (byte order)'
out=$(printf 'b 10\na 9\n' | ../simpsh -c 'sort -k 2')
if [ "$out" = "b 10
a 9" ]; then
  msg_pass "sort -k 2 byte order"
else
  msg_fail "sort -k 2: expected 'b 10' before 'a 9' (byte compare, not numeric)"
  exit 1
fi

msg_run 'sort -k 2.2,2.2 (char offset)'
out=$(printf 'x b\ny a\n' | ../simpsh -c 'sort -k 2.2,2.2')
if [ "$out" = "y a
x b" ]; then
  msg_pass "sort -k 2.2,2.2"
else
  msg_fail "sort -k 2.2,2.2: expected 'y a' before 'x b' (compare 2nd char of field 2)"
  exit 1
fi

msg_run 'sort -k 2.2b,2.2b (b modifier)'
out=$(printf 'xx  dc\nxx  ab\n' | ../simpsh -c 'sort -k 2.2b,2.2b')
if [ "$out" = "xx  ab
xx  dc" ]; then
  msg_pass "sort -k 2.2b,2.2b"
else
  msg_fail "sort -k 2.2b,2.2b: b modifier must skip leading blanks in the field"
  exit 1
fi

msg_run 'sort -k 2 (leading blanks)'
out=$(printf 'x a\nx  b\n' | ../simpsh -c 'sort -k 2')
if [ "$out" = "x  b
x a" ]; then
  msg_pass "sort -k 2 leading blanks"
else
  msg_fail "sort -k 2: without -b, leading blanks take part in the byte order"
  exit 1
fi

msg_run 'sort -b -k 2'
out=$(printf 'x a\nx  b\n' | ../simpsh -c 'sort -b -k 2')
if [ "$out" = "x a
x  b" ]; then
  msg_pass "sort -b -k 2"
else
  msg_fail "sort -b -k 2: global -b must ignore leading blanks of the key field"
  exit 1
fi

msg_run 'sort -r'
out=$(printf 'a\nb\n' | ../simpsh -c 'sort -r')
if [ "$out" = "b
a" ]; then
  msg_pass "sort -r"
else
  msg_fail "sort -r: expected 'b' before 'a' (reverse)"
  exit 1
fi

msg_run 'sort -c -k 2 (field includes separator)'
printf 'y\tb\nx a\n' | ../simpsh -c 'sort -c -k 2' 2>/dev/null
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "0 (already sorted)" ""
else
  test_fail "rc" "expected 0" "$rc"
  exit 1
fi

msg_run 'sort -c -k 2 (disorder)'
printf 'a 2\nb 1\n' | ../simpsh -c 'sort -c -k 2' 2>/dev/null
rc=$?
if [ $rc -eq 1 ]; then
  test_pass "rc" "1 (disorder)" ""
else
  test_fail "rc" "expected 1" "$rc"
  exit 1
fi

msg_run 'sort -C -k 2 (silent)'
printf 'a 2\nb 1\n' | ../simpsh -c 'sort -C -k 2' 2>/dev/null
rc=$?
if [ $rc -eq 1 ]; then
  test_pass "rc" "1 (disorder)" ""
else
  test_fail "rc" "expected 1" "$rc"
  exit 1
fi

msg_run 'sort -t : -k 3,3n'
out=$(printf 'user:x:1000:1000:user:/home/user:/bin/sh\ndaemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin\nroot:x:0:0:root:/root:/bin/sh\n' | ../simpsh -c 'sort -t : -k 3,3n')
if [ "$out" = "root:x:0:0:root:/root:/bin/sh
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
user:x:1000:1000:user:/home/user:/bin/sh" ]; then
  msg_pass "sort -t : -k 3,3n"
else
  msg_fail "sort -t : -k 3,3n: numeric compare on : field 3 (root, daemon, user)"
  exit 1
fi

msg_run 'sort -k 2,2 -k 1,1 (multiple keys)'
out=$(printf 'b 2\na 2\n' | ../simpsh -c 'sort -k 2,2 -k 1,1')
if [ "$out" = "a 2
b 2" ]; then
  msg_pass "sort -k 2,2 -k 1,1"
else
  msg_fail "sort -k 2,2 -k 1,1: equal field 2 must fall back to field 1"
  exit 1
fi

msg_run 'sort -u'
out=$(printf 'b\na\nb\n' | ../simpsh -c 'sort -u')
if [ "$out" = "a
b" ]; then
  msg_pass "sort -u"
else
  msg_fail "sort -u: duplicate 'b' must be suppressed"
  exit 1
fi

msg_run 'sort -o outfile'
printf 'b\na\n' | ../simpsh -c "sort -o $tmpout"
out=$(cat "$tmpout")
rm -f "$tmpout"
if [ "$out" = "a
b" ]; then
  msg_pass "sort -o"
else
  msg_fail "sort -o: expected sorted output written to the file"
  exit 1
fi

msg_run 'sort -m (merge)'
printf 'a\nc\n' > "$tmpm1"
printf 'b\nd\n' > "$tmpm2"
out=$(../simpsh -c "sort -m $tmpm1 $tmpm2")
rm -f "$tmpm1" "$tmpm2"
if [ "$out" = "a
b
c
d" ]; then
  msg_pass "sort -m"
else
  msg_fail "sort -m: expected merged order a, b, c, d"
  exit 1
fi

msg_run 'sort -d (dictionary)'
out=$(printf 'x!2\nx1\n' | ../simpsh -c 'sort -d')
if [ "$out" = "x1
x!2" ]; then
  msg_pass "sort -d"
else
  msg_fail "sort -d: '!' must be ignored so 'x1' sorts before 'x!2'"
  exit 1
fi

msg_run 'sort -f (fold case)'
out=$(printf 'B\na\n' | ../simpsh -c 'sort -f')
if [ "$out" = "a
B" ]; then
  msg_pass "sort -f"
else
  msg_fail "sort -f: case must be folded so 'a' sorts before 'B'"
  exit 1
fi

msg_run 'sort -i (ignore non-printable)'
out=$(printf 'aa\na\001z\n' | ../simpsh -c 'sort -i')
exp=$(printf 'aa\na\001z')
if [ "$out" = "$exp" ]; then
  msg_pass "sort -i"
else
  msg_fail "sort -i: non-printable chars must be ignored in comparison"
  exit 1
fi

msg_run 'sort -k 2 (field past EOL)'
out=$(printf 'a\nb c\n' | ../simpsh -c 'sort -k 2')
if [ "$out" = "a
b c" ]; then
  msg_pass "sort -k 2 field past EOL"
else
  msg_fail "sort -k 2: a missing field must sort before a present one"
  exit 1
fi

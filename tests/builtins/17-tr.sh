#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'tr maps a-z to A-Z'
out=$(printf 'abc\n' | ../simpsh -c "tr a-z A-Z")
if [ "$out" = "ABC" ]; then
  test_pass "out" "matches" "ABC"
else
  test_fail "out" "expected" "ABC"
  exit 1
fi

msg_run 'tr mapping follows written order not byte order'
out=$(printf 'abc\n' | ../simpsh -c "tr 'ba' 'xy'")
if [ "$out" = "yxc" ]; then
  test_pass "out" "matches" "yxc"
else
  test_fail "out" "expected" "yxc"
  exit 1
fi

msg_run 'tr duplicate source last-wins'
out=$(printf 'abc\n' | ../simpsh -c "tr 'aba' 'xyz'")
if [ "$out" = "zyc" ]; then
  test_pass "out" "matches" "zyc"
else
  test_fail "out" "expected" "zyc"
  exit 1
fi

msg_run 'tr range brackets'
out=$(printf 'abcde\n' | ../simpsh -c "tr '[a-e]' '[A-E]'")
if [ "$out" = "ABCDE" ]; then
  test_pass "out" "matches" "ABCDE"
else
  test_fail "out" "expected" "ABCDE"
  exit 1
fi

msg_run 'tr character classes down'
out=$(printf 'ABC123\n' | ../simpsh -c "tr '[:upper:]' '[:lower:]'")
if [ "$out" = "abc123" ]; then
  test_pass "out" "matches" "abc123"
else
  test_fail "out" "expected" "abc123"
  exit 1
fi

msg_run 'tr character classes up'
out=$(printf 'A\n' | ../simpsh -c "tr '[:upper:]' '[:lower:]'")
if [ "$out" = "a" ]; then
  test_pass "out" "matches" "a"
else
  test_fail "out" "expected" "a"
  exit 1
fi

msg_run 'tr repeat in string2'
out=$(printf 'ab\n' | ../simpsh -c "tr 'a' '[x*2]'")
if [ "$out" = "xb" ]; then
  test_pass "out" "matches" "xb"
else
  test_fail "out" "expected" "xb"
  exit 1
fi

msg_run 'tr equivalence class'
out=$(printf 'a\n' | ../simpsh -c "tr '[=a=]' x")
if [ "$out" = "x" ]; then
  test_pass "out" "matches" "x"
else
  test_fail "out" "expected" "x"
  exit 1
fi

msg_run 'tr -c complements in byte order'
out=$(printf 'abc123\n' | ../simpsh -c "tr -c '0-9' X")
if [ "$out" = "XXX123X" ]; then
  test_pass "out" "matches" "XXX123X"
else
  test_fail "out" "expected" "XXX123X"
  exit 1
fi

msg_run 'tr -c pads with last set2 char'
out=$(printf 'ab\n' | ../simpsh -c "tr -c a xy")
if [ "$out" = "ayy" ]; then
  test_pass "out" "matches" "ayy"
else
  test_fail "out" "expected" "ayy"
  exit 1
fi

msg_run 'tr -C behaves like -c'
out=$(printf 'ab\n' | ../simpsh -c "tr -C a xy")
if [ "$out" = "ayy" ]; then
  test_pass "out" "matches" "ayy"
else
  test_fail "out" "expected" "ayy"
  exit 1
fi

msg_run 'tr -d deletes'
out=$(printf 'aabbcc\n' | ../simpsh -c "tr -d a")
if [ "$out" = "bbcc" ]; then
  test_pass "out" "matches" "bbcc"
else
  test_fail "out" "expected" "bbcc"
  exit 1
fi

msg_run 'tr -d with range'
out=$(printf 'a1b2\n' | ../simpsh -c "tr -d '0-9'")
if [ "$out" = "ab" ]; then
  test_pass "out" "matches" "ab"
else
  test_fail "out" "expected" "ab"
  exit 1
fi

msg_run 'tr -s squeezes'
out=$(printf 'aabb\n' | ../simpsh -c "tr -s a")
if [ "$out" = "abb" ]; then
  test_pass "out" "matches" "abb"
else
  test_fail "out" "expected" "abb"
  exit 1
fi

msg_run 'tr -s with mapping squeezes output'
out=$(printf 'aabbcc\n' | ../simpsh -c "tr -s bc x")
if [ "$out" = "aax" ]; then
  test_pass "out" "matches" "aax"
else
  test_fail "out" "expected" "aax"
  exit 1
fi

msg_run 'tr -ds'
out=$(printf 'aabbcc\n' | ../simpsh -c "tr -ds a b")
if [ "$out" = "bcc" ]; then
  test_pass "out" "matches" "bcc"
else
  test_fail "out" "expected" "bcc"
  exit 1
fi

msg_run 'tr -cs'
out=$(printf 'aabb\n' | ../simpsh -c "tr -cs a x")
if [ "$out" = "aax" ]; then
  test_pass "out" "matches" "aax"
else
  test_fail "out" "expected" "aax"
  exit 1
fi

msg_run 'tr -cds'
out=$(printf 'a12b\n' | ../simpsh -c "tr -cds '0-9' ' '")
if [ "$out" = "12" ]; then
  test_pass "out" "matches" "12"
else
  test_fail "out" "expected" "12"
  exit 1
fi

msg_run 'tr delete happens before squeeze in output stream'
out=$(printf 'axa' | ../simpsh -c "tr -ds x a")
if [ "$out" = "a" ]; then
  test_pass "out" "matches" "a"
else
  test_fail "out" "expected" "a"
  exit 1
fi

msg_run 'tr squeeze survives buffer boundary'
n=$(head -c 20000 /dev/zero | ../simpsh -c "tr -s '\0'" | wc -c)
if [ "$n" = "1" ]; then
  test_pass "n" "matches" "1"
else
  test_fail "n" "expected" "1"
  exit 1
fi

msg_run 'tr translate across buffer boundary'
n=$(yes ab | head -c 20000 | ../simpsh -c "tr ab xy" | wc -c)
if [ "$n" = "20000" ]; then
  test_pass "n" "matches" "20000"
else
  test_fail "n" "expected" "20000"
  exit 1
fi

msg_run 'tr missing operand'
out=$(../simpsh -c 'tr' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr extra operand'
out=$(../simpsh -c 'tr a b c' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr -d missing operand'
out=$(../simpsh -c 'tr -d' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr -ds missing operand'
out=$(../simpsh -c 'tr -ds a' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr -c missing operand'
out=$(../simpsh -c 'tr -c' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr class not valid in string2'
out=$(../simpsh -c "tr a '[[:alpha:]]'" 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr pairing with empty string1'
out=$(../simpsh -c "tr '' '[:upper:]'" 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr pairing requires matching class in string1'
out=$(../simpsh -c "tr '[:alpha:]' '[:upper:]'" 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr reversed range'
out=$(../simpsh -c "tr 'z-a' x" 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'tr bad option'
out=$(../simpsh -c 'tr -q a b' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

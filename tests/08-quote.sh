#!/bin/sh
# shellcheck disable=2116
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

# === Single quotes are literal — no expansion ===
msg_run "single quote test: echo '\$var'"
out=$(../simpsh -c "echo '\$var'")
if [ "$out" != '$var' ]; then
  test_fail "out" "expected" '$var'
  exit 1
else
  test_pass "out" "matches" '$var'
fi

# === Double quotes preserve whitespace but expand $var ===
msg_run 'double quote test: var="a b c"; echo "$var"'
out1=$(../simpsh -c 'var="a b c"; echo "$var"')
if [ "$out1" != "a b c" ]; then
  test_fail "out1" "expected" "a b c"
  msg_fail "\$out1=$out1 differs from 'a b c'"
  exit 1
else
  test_pass "out1" "matches" "a b c"
fi

# === Backslash escapes the next char (space stays in one word) ===
msg_run "backlash escape test: echo a\\\ b"
out2=$(../simpsh -c 'echo a\ b')
if [ "$out2" != "a b" ]; then
  test_fail "out2" "expected" "a b"
  exit 1
else
  test_pass "out2" "matches" "a b"
fi

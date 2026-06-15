#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run '; separated command test: "printf '%s' a; printf '%s' b; printf '%s' c"'
out=$(../simpsh -c "printf '%s' a; printf '%s' b; printf '%s' c")

if [ "$out" != abc ]; then
  test_fail "out" "expected" "abc"
  exit 1
else
  test_pass  "out" "matches" "abc"
fi

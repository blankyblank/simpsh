#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'echo foo=bar; echo $foo | ../simpsh'
out=$(echo 'foo=bar; echo $foo' | ../simpsh )

if  [ "$out" = "bar" ]; then
  test_pass  "out" "matches" "bar"
else
  test_fail  "out" "expected" "bar"
  exit 1
fi

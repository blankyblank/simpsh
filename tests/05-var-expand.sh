#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run 'echo foo=bar; echo $foo | ../simpsh'
out=$(echo 'foo=bar; echo $foo' | ../simpsh )

if  [ "$out" = "bar" ]; then
  test_pass  "out" "matches" "bar"
else
  msg_fail "\$out=$out differs from 'bar'"
  exit 1
fi

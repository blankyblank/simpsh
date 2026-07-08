#!/bin/sh

# shellcheck disable=2016
# shellcheck disable=2164

[ -f ./funcs ] && . ./funcs

msg_run 'glob * test 2: echo *.test'
out1=$(simpsh -c 'echo *.test')
if [ "$out1" = "a.test b.test" ]; then
  test_pass "out1" "matches" 'a.test b.test'
else
  test_fail "out1" "expected" 'a.test b.test'
  exit 1
fi

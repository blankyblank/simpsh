#!/bin/sh
# shellcheck disable=2116
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'command substitution test: t=$(echo test123) ; echo $t'
out=$(../simpsh -c 't=$(echo test123) ; echo $t')
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi

msg_run 'nested command substitution test: t=$(echo $(echo test123)); echo $t'
out1=$(../simpsh -c 't=$(echo $(echo test123)); echo $t')
if [ "$out1" != "test123" ]; then
  test_fail "out1" "expected" "test123"
  exit 1
else
  test_pass "out1" "matches" "test123"
fi

msg_run 'command substitution test 2: echo before_$(echo in)_after'
out2=$(../simpsh -c 'echo before_$(echo in)_after')
if [ "$out2" != "before_in_after" ]; then
  test_fail "out2" "expected" "before_in_after"
  exit 1
else
  test_pass "out2" "matches" "before_in_after"
fi

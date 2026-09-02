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

msg_run 'backtick command substitution test: t=`echo test123` ; echo $t'
out3=$(../simpsh -c 't=`echo test123` ; echo $t')
if [ "$out3" != "test123" ]; then
  test_fail "out3" "expected" "test123"
  exit 1
else
  test_pass "out3" "matches" "test123"
fi

msg_run "cmdsub with pipe: t=\$(echo hi | tr a-z A-Z); echo \$t"
out=$(../simpsh -c 't=$(echo hi | tr a-z A-Z); echo $t')
if [ "$out" != "HI" ]; then
  test_fail "out" "expected" "HI"; exit 1
else
  test_pass "out" "matches" "HI"
fi

msg_run 'cmdsub with for loop: x=$(for i in a b; do echo $i; done); echo "$x"'
out=$(../simpsh -c 'x=$(for i in a b; do echo $i; done); echo "$x"')
if [ "$out" != "a
b" ]; then
  test_fail "out" "expected" "a b"; exit 1
else
  test_pass "out" "matches" "a b"
fi

msg_run 'cmdsub multi-line: x=$(echo a; echo b); echo "$x"'
out=$(../simpsh -c 'x=$(echo a; echo b); echo "$x"')
if [ "$out" != "a
b" ]; then
  test_fail "out" "expected" "a b"; exit 1
else
  test_pass "out" "matches" "a b"
fi

msg_run 'backtick with pipe: t=`echo hi | tr a-z A-Z`; echo $t'
out=$(../simpsh -c 't=`echo hi | tr a-z A-Z`; echo $t')
if [ "$out" != "HI" ]; then
  test_fail "out" "expected" "HI"; exit 1
else
  test_pass "out" "matches" "HI"
fi

msg_run 'case inside cmdsub: $(case x in a) echo hi;; esac)'
export x=a
out=$(../simpsh -c 'echo $(case $x in a) echo hi;; esac)')
if [ "$out" !=  'hi' ]; then
  test_fail "out" "expected" "hi"; exit 1
else
  test_pass "out" "matches" "hi"
fi

msg_run 'cmdsub in ${var:-...} with suffix (mdev regression)'
out=$(../simpsh -c 'echo ${_undef:-$(echo a)}b')
if [ "$out" = "ab" ]; then
  test_pass "out" "matches" "ab"
else
  test_fail "out" "expect" "ab"
  exit 1
fi


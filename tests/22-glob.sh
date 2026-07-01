#!/bin/sh

# shellcheck disable=2016
# shellcheck disable=2164

[ -f ./funcs ] && . ./funcs

# out=$(../simpsh -c 'i=0; i=$((i + 1)); echo $i')
# [ "$out" = 1 ] || exit 1

d=$(mktemp -d)
cd $d
msg_run 'glob * test 1: echo *'
out=$(simpsh -c 'echo *')
if [ "$out" = "*" ]; then
  test_pass "out" "matches" '*'
else
  test_fail "out" "expected" '*'
  rm -rf $d
  exit 1
fi

touch a.test b.test
msg_run 'glob * test 2: echo *.test'
out1=$(simpsh -c 'echo *.test')
if [ "$out1" = "a.test b.test" ]; then
  test_pass "out1" "matches" 'a.test b.test'
else
  test_fail "out1" "expected" 'a.test b.test'
  rm -rf $d
  exit 1
fi

msg_run 'glob ? test: echo ?.test'
out2=$(simpsh -c 'echo ?.test')
if [ "$out2" = "a.test b.test" ]; then
  test_pass "out2" "matches" 'a.test b.test'
else
  test_fail "out2" "expected" 'a.test b.test'
  rm -rf $d
  exit 1
fi
msg_run 'glob [a-z] test: echo [a-z].test'
out2=$(simpsh -c 'echo [a-z].test')
if [ "$out2" = "a.test b.test" ]; then
  test_pass "out2" "matches" 'a.test b.test'
else
  test_fail "out2" "expected" 'a.test b.test'
  rm -rf $d
  exit 1
fi
rm -rf $d

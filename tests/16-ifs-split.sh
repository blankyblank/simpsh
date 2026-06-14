#!/bin/sh
# shellcheck disable=2116
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'echo a b c'
out=$(../simpsh -c 'echo a b c')
if [ "$out" != "a b c" ]; then
  test_fail "out" "not equal to" "a b c"
  exit 1
else
  test_pass "out" "matches" "a b c"
fi

msg_run 'var="a b c"; set -- $var; echo $#'
out1=$(../simpsh -c 'var="a b c"; set -- $var; echo $#')
if [ "$out1" -ne 3 ]; then
  test_fail "out1" "not equal to" "3"
  exit 1
else
  test_pass "out1" "number of args is" "3"
fi

msg_run 'var="a b c"; set -- "$var"; echo $#'
out2=$(../simpsh -c 'var="a b c"; set -- "$var"; echo $#')
if [ "$out2" -ne 1 ]; then
  test_fail "out2" "not equal to" "1"
  exit 1
else
  test_pass "out2" "number of args is" "1"
fi

msg_run 'IFS= ; var="a b c"; set -- $var; echo $#'
out3=$(../simpsh -c 'IFS= ; var="a b c"; set -- $var; echo $#')
if [ "$out3" -ne 1 ]; then
  test_fail "out3" "not equal to" "1"
  exit 1
else
  test_pass "out3" "number of args is" "1"
fi

msg_run 'IFS=: ; var="a:b:c"; set -- $var; echo $#'
out4=$(../simpsh -c 'IFS=: ; var="a:b:c"; set -- $var; echo $#')
if [ "$out4" -ne 3 ]; then
  test_fail "out4" "not equal to" "3"
  exit 1
else
  test_pass "out4" "number of args is" "3"
fi

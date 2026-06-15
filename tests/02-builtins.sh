#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'echo test: "echo Test test!"'
out=$(../simpsh -c "echo Test test!")

if [ "$out" != "Test test!" ]; then
  test_fail "out" "differs from" "Test test!"
  exit 1
else
  test_pass "out" "matches" "Test test!"
fi

msg_run 'cd/pwd test: "cd /tmp ; pwd"'
out=$(../simpsh -c "cd /tmp ; pwd")

if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"
  exit 1
else
  test_pass "out" "matches" "/tmp"
fi

msg_run 'exit builtin test 1: "exit 0"; echo $?'
zero=$(../simpsh -c "exit 0"; echo $?)
if [ "$zero" -eq 0 ]; then
  msg_pass "\$zero=$zero correct exit status"
else
  msg_fail "exit 0 produced incorrect value"
  exit 1
fi

msg_run 'exit builtin test 2: exit 5" ; echo $?'
five=$(../simpsh -c "exit 5" ; echo $?)
if [ "$five" -eq 5 ]; then
  msg_pass "\$five=$five correct exit status"
else
  msg_fail "exit 5 produced incorrect value"
  exit 1
fi


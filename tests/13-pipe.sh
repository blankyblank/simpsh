#!/bin/sh
# shellcheck disable=2181

[ -f ./funcs ] && . ./funcs

../simpsh -c "{ echo true"
msg_run "pipe test: ls | grep 13-pipe.sh"
out=$(../simpsh -c "ls | grep 13-pipe.sh")
if [ "$out" != "13-pipe.sh" ]; then
  test_fail "out" "expected" "13-pipe.sh"
  exit 1
else
  test_pass "out" "matches" "13-pipe.sh"
fi

msg_run "pipe exit status test: echo test | cat | grep fake ; echo \$?"
out1=$(../simpsh -c 'echo test | cat | grep fake ; echo $?')
if [ "$out1" -eq 0 ]; then
  test_fail "out1" "exit status should have been" "1"
  exit 1
else
  test_pass "out1" "exit status is not" "0"
fi

msg_run "pipe negation test 1: ! echo foo | cat >/dev/null; echo \$?"
out2=$(../simpsh -c '! echo foo | cat >/dev/null; echo $?')
if [ "$out2" -eq 0 ]; then
  test_fail "out2" "exit status should have been" "1"
  exit 1
else
  test_pass "out2" "exit status is not" "0"
fi

msg_run "pipe negation test 2: echo foo | ! cat; echo \$?"
../simpsh -c 'echo foo | ! cat ; echo $?' 2>/dev/null
if [ "$?" -ne 2 ]; then
  msg_fail "exit status should have been 2"
  exit 1
else
  msg_pass "exit status is 2"
fi

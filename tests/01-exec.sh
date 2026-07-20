#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run 'nonzero status: "false; echo $?"'
_fail_s=$(../simpsh -c 'false; echo $?')
if [ "$_fail_s" -eq 0 ]; then
  test_fail "_fail_s" "exit status is" "0"
  exit 1
else
  test_pass "_fail_s" "exit status isn't" "0"
fi

msg_run 'zero status: "true; echo $?"'
_suc_s=$(../simpsh -c 'true; echo $?')
if [ "$_suc_s" -ne 0 ]; then
  test_fail "_suc_s" "isn't" "0"
  exit 1
else
  test_pass "_suc_s" "exit status is" "0"
fi

msg_run 'wait for background job'
out=$(../simpsh -c 'sleep 0.1 & wait; echo done')
if [ "$out" = "done" ]; then
  test_pass "out" "matches" "done"
else
  test_fail "out" "expected" "done"
  exit 1
fi

msg_run 'wait exit status'
out=$(../simpsh -c '(exit 3) & wait $!; echo $?')
if [ "$out" = "3" ]; then
  test_pass "out" "matches" "3"
else
  test_fail "out" "expected" "3"
  exit 1
fi

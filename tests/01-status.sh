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

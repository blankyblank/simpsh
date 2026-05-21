#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }


msg_run '"false; echo $?"'
_fail_s=$(../simpsh -c 'false; echo $?')
if [ "$_fail_s" -eq 0 ]; then
  msg_fail "\$_fail_s: $_fail_s should be nonzero"
  exit 1
else
  msg_pass "\$_fail_s=$_fail_s exit status is nonzero"
fi

msg_run '"true; echo $?"'
_suc_s=$(../simpsh -c 'true; echo $?')
if [ "$_suc_s" -ne 0 ]; then
  msg_fail "\$_suc_s: $_suc_s should be zero"
  exit 1
else
  msg_pass "\$_suc_s=$_suc_s exit status is zero"
fi

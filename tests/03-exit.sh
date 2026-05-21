#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
# shellcheck disable=2181

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run '"exit 0"; echo $?'
zero=$(../simpsh -c "exit 0"; echo $?)
if [ "$zero" -eq 0 ]; then
  msg_pass "\$zero=$zero correct exit status"
else
  msg_fail "exit 0 produced incorrect value"
  exit 1
fi

msg_run 'exit 5" ; echo $?'
five=$(../simpsh -c "exit 5" ; echo $?)
if [ "$five" -eq 5 ]; then
  msg_pass "\$five=$five correct exit status"
else
  msg_fail "exit 5 produced incorrect value"
  exit 1
fi

exit 0

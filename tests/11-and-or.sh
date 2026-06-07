#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }


msg_run '"false && echo true || echo false"'
out1=$(../simpsh -c "false && echo true || echo false")
if [ "$out1" != false ]; then
  msg_fail "\$out1=$out1 differs from 'false'"
  exit 1
else
  test_pass  "out1" "matches" "false"
fi

msg_run '"true && echo true || echo false"'
out2=$(../simpsh -c "true && echo true || echo false")
if [ "$out2" != true ]; then
  msg_fail "\$out2: $out2 differs from true"
  exit 1
else
  test_pass  "out2" "matches" "true"
fi

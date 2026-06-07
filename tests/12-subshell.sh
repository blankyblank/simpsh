#!/bin/sh
### shellcheck #disable=2015
# set -x

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run "running (echo test123)"
out=$(../simpsh -c "(echo test123)")
if [ "$out" != "test123" ]; then
  msg_fail "\$out=$out differs from 'test123'"
  exit 1
else
  test_pass "out" "matches" "test123"
fi


msg_run "running (echo test123)"
out2=$(../simpsh -c "(echo -n a ; echo b)")
if [ "$out2" != "ab" ]; then
  msg_fail "\$out2=$out2 differs from 'ab'"
  exit 1
else
  test_pass  "out2" "matches" "ab"
fi

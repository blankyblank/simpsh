#!/bin/sh
### shellcheck #disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run "while loop 0 to 1000"

out=$(./simpsh -c 'i=0; while [ $i -lt 100000 ]; do i=$((i+1)); done')
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

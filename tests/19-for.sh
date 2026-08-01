#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'for loop test 1: for f in *; do echo $f; done'
out1=$(../simpsh -c 'for f in *; do echo $f; done' | grep '19-for.sh')
if [ "$out1" = "19-for.sh" ]; then
  test_pass "out1" "matches" "19-for.sh"
else
  test_fail "out1" "expected" "19-for.sh"
  exit 1
fi

msg_run 'for loop test 2: s=""; for f in a b c; do s="$s$f"; done; echo $s'
out2=$(../simpsh -c 's=""; for f in a b c; do s="$s$f"; done; echo $s')
if [ "$out2" = "abc" ]; then
  test_pass "out2" "matches" "abc"
else
  test_fail "out2" "expected" "abc"
  exit 1
fi

msg_run 'for loop test 3: set -- a b c; s=""; for f; do s="$s$f"; done; echo $s'
out3=$(../simpsh -c 'set -- a b c; s=""; for f; do s="$s$f"; done; echo $s')
if [ "$out3" = "abc" ]; then
  test_pass "out3" "matches" "abc"
else
  test_fail "out3" "expected" "abc"
  exit 1
fi


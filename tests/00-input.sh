#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run "stdin test: printf '%s\n' "echo test123" | ../simpsh"
out=$(printf '%s\n' "echo test123" | ../simpsh)
if [ $out = test123 ]; then
  test_pass "out" "matches" "test123"
else
  test_fail "out" "differs from" "test123"
  exit 1
fi

msg_run "-c string test: ../simpsh -c 'echo test123'"
out1=$(../simpsh -c 'echo test123')
if [ $out1 = test123 ]; then
  test_pass "out1" "matches" "test123"
else
  test_fail "out" "differs from" "test123"
  exit 1
fi

msg_run "script file test: ../simpsh ./testfiles/input.sh"
out2=$(../simpsh ./testfiles/input.sh)
if [ $out2 = test123 ]; then
  test_pass "out2" "matches" "test123"
else
  test_fail "out2" "differs from" "test123"
  exit 1
fi

msg_run "script shebang test: ../simpsh -c ./testfiles/input.sh"
out3=$(../simpsh ./testfiles/input.sh)
if [ $out3 = test123 ]; then
  test_pass "out3" "matches" "test123"
else
  test_fail "out3" "differs from" "test123"
  exit 1
fi


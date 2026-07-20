#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run 'trap INT: kill -INT $$ from subshell'
out=$(../simpsh -c 'trap "echo trapped" INT; kill -INT $$; echo alive')
if [ "$out" = "alive
trapped" ]; then
  test_pass "out" "matches" "trapped"
else
  test_fail "out" "expected" "trapped"
  exit 1
fi

msg_run 'trap EXIT: exit handler fires'
out=$(../simpsh -c 'trap "echo bye" EXIT; echo -n "hello "')
if [ "$out" = "hello bye" ]; then
  test_pass "out" "matches" "hello bye"
else
  test_fail "out" "expected" "hello bye"
  exit 1
fi

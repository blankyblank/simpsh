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

msg_run 'trap \"\" QUIT: ignore SIGQUIT'
out=$(../simpsh -c 'trap "" QUIT; kill -QUIT $$; echo survived')
if [ "$out" = "survived" ]; then
  test_pass "out" "matches" "survived"
else
  test_fail "out" "expected" "survived"
  exit 1
fi

msg_run 'trap - QUIT: reset to default, shell dies'
../simpsh -c 'trap "" QUIT; trap - QUIT; kill -QUIT $$' 2>/dev/null
rc=$?
if [ "$rc" -ge 128 ]; then
  test_pass "rc" "matches" ">=128"
else
  test_fail "rc" "expected" ">=128"
  exit 1
fi

msg_run 'trap QUIT: reset to default (no action)'
../simpsh -c 'trap "" QUIT; trap QUIT; kill -QUIT $$' 2>/dev/null
rc=$?
if [ "$rc" -ge 128 ]; then
  test_pass "rc" "matches" ">=128"
else
  test_fail "rc" "expected" ">=128"
  exit 1
fi

msg_run 'trap \"echo x\" INT TERM: multiple signals'
out=$(../simpsh -c 'trap "echo caught" INT TERM; kill -INT $$; echo alive')
if [ "$out" = "alive
caught" ]; then
  test_pass "out" "matches" "caught"
else
  test_fail "out" "expected" "caught"
  exit 1
fi

msg_run 'trap with no args: lists set traps'
out=$(../simpsh -c 'trap "echo x" INT; trap')
if echo "$out" | grep -q INT; then
  test_pass "out" "contains" "INT"
else
  test_fail "out" "expected to contain" "INT"
  exit 1
fi

msg_run 'EXIT trap preserves non-zero exit'
../simpsh -c 'trap "true" EXIT; false' >/dev/null 2>&1
rc=$?
if [ "$rc" = "1" ]; then
  test_pass "rc" "matches" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'trap reset in subshell does not leak to parent'
out=$(../simpsh -c 'trap "" TERM; (trap - TERM); kill -TERM $$; echo survived')
if [ "$out" = "survived" ]; then
  test_pass "out" "matches" "survived"
else
  test_fail "out" "expected" "survived"
  exit 1
fi

msg_run 'trap \"\" PIPE: set (no crash)'
out=$(../simpsh -c 'trap "" PIPE; echo ok')
if [ "$out" = "ok" ]; then
  test_pass "out" "matches" "ok"
else
  test_fail "out" "expected" "ok"
  exit 1
fi

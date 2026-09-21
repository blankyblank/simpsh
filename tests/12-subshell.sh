#!/bin/sh
# shellcheck disable=2016,2181

[ -f ./funcs ] && . ./funcs

msg_run "subshell test 1: (echo test123)"
out=$(../simpsh -c "(echo test123)")
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi


# need to find a better second test than this. maybe exit status or something
msg_run "subshell test 2: (echo -n a ; echo b)"
out2=$(../simpsh -c "(echo -n a ; echo b)")
if [ "$out2" != "ab" ]; then
  test_fail "out2" "expected" "ab"
  exit 1
else
  test_pass  "out2" "matches" "ab"
fi

msg_run 'subshell isolates vars and cwd'
out=$(../simpsh -c 'v=outer; (v=inner; cd /tmp); echo "v=$v"')
if [ "$out" = "v=outer" ]; then test_pass "out" "matches" "v=outer"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'subshell exit status propagates'
../simpsh -c '(exit 3)'
if [ "$?" = "3" ]; then test_pass "rc" "matches" "3"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'subshell trap does not leak to parent'
out=$(../simpsh -c 'trap "" TERM; (trap - TERM); trap | grep -c TERM')
if [ "$out" = "1" ]; then test_pass "out" "matches" "1"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'EXIT trap fires in subshell'
out=$(../simpsh -c '(trap "echo sub-exit" EXIT)')
# => sub-exit

msg_run 'subshell EXIT trap preserves exit status'
out=$(../simpsh -c '(trap "echo x" EXIT; exit 3)')
rc=$?
# out=x rc=3  (check both!)

msg_run 'parent EXIT trap not fired by subshell end'
out=$(../simpsh -c 'trap "echo parent" EXIT; (echo in-sub); echo end')
# => in-sub / end / parent in order

msg_run 'EXIT trap fires in bg subshell'
out=$(../simpsh -c '(trap "echo x" EXIT; echo hi) & wait')
# => hi + x (order-agnostic grep, like the wait+HUP test)

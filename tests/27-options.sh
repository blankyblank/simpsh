#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run 'set -e: exit on error'
out=$(../simpsh -c 'set -e; false; echo "survived"' 2>&1)
rc=$?
if [ $rc -ne 0 ]; then
  test_pass "rc" "non-zero (exited)" ""
else
  test_fail "rc" "expected non-zero" "$rc"
  exit 1
fi

msg_run 'set +e: no exit on error'
out=$(../simpsh -c 'set +e; false; echo survived')
if [ "$out" = "survived" ]; then
  test_pass "out" "survived" ""
else
  test_fail "out" "expected" "survived"
  exit 1
fi

msg_run 'set -u: error on unset variable'
out=$(../simpsh -c 'set -u; echo $unsetvar' 2>&1)
rc=$?
if [ $rc -ne 0 ]; then
  test_pass "rc" "non-zero" ""
else
  test_fail "rc" "expected non-zero" "$rc"
  exit 1
fi

msg_run 'set -f: disable globbing'
out=$(../simpsh -c 'set -f; echo *')
if [ "$out" = "*" ]; then
  test_pass "out" "matches" "*"
else
  test_fail "out" "expected" "*"
  exit 1
fi

msg_run 'set +f: re-enable globbing'
out=$(../simpsh -c 'set +f; echo *')
if [ "$out" != "*" ]; then
  msg_pass "contains files"
else
  test_fail "out" "expected glob expansion" ""
  exit 1
fi

msg_run "xtrace PS4 goes to stderr: set -x; echo hi"
out=$(../simpsh -c 'set -x; echo hi' 2>/dev/null)
err=$(../simpsh -c 'set -x; echo hi' 2>&1 >/dev/null)
if [ "$out" != "hi" ] || [ "$err" != "+ echo hi" ]; then
  test_fail "out/err" "expected" "hi / + echo hi"; exit 1
else
  test_pass "out/err" "matches" "hi / + echo hi"
fi

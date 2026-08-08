#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

tmpr=/tmp/realpath.$$

msg_run 'realpath resolves to absolute path'
touch "$tmpr"
out=$(../simpsh -c "realpath $tmpr")
rm -f "$tmpr"
if [ "$out" = "$tmpr" ]; then
  test_pass "out" "matches" "$tmpr"
else
  test_fail "out" "expected" "$tmpr"
  exit 1
fi

msg_run 'realpath resolves .. components'
out=$(../simpsh -c 'realpath /tmp/../tmp')
if [ "$out" = "/tmp" ]; then
  test_pass "out" "matches" "/tmp"
else
  test_fail "out" "expected" "/tmp"
  exit 1
fi

msg_run 'realpath -q (quiet success)'
touch "$tmpr"
out=$(../simpsh -c "realpath -q $tmpr")
rc=$?
rm -f "$tmpr"
if [ "$out" = "$tmpr" ] && [ $rc -eq 0 ]; then
  test_pass "out" "matches" "$tmpr"
else
  test_fail "out" "expected" "$tmpr"
  exit 1
fi

msg_run 'realpath -q suppresses error message'
out=$(../simpsh -c 'realpath -q /nonexistent' 2>&1)
rc=$?
if [ -z "$out" ] && [ $rc -eq 1 ]; then
  test_pass "out" "quiet (no output)" ""
else
  test_fail "out" "expected quiet (no output)" ""
  exit 1
fi

msg_run 'realpath missing prints error'
out=$(../simpsh -c 'realpath /nonexistent' 2>&1)
rc=$?
if [ -n "$out" ] && [ $rc -eq 1 ]; then
  test_pass "rc" "error message on stderr" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

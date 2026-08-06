#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'dirname /usr/bin/foo'
out=$(../simpsh -c 'dirname /usr/bin/foo')
if [ "$out" = "/usr/bin" ]; then
  test_pass "out" "matches" "/usr/bin"
else
  test_fail "out" "expected" "/usr/bin"
  exit 1
fi

msg_run 'dirname foo'
out=$(../simpsh -c 'dirname foo')
if [ "$out" = "." ]; then
  test_pass "out" "matches" "."
else
  test_fail "out" "expected" "."
  exit 1
fi

msg_run 'dirname /'
out=$(../simpsh -c 'dirname /')
if [ "$out" = "/" ]; then
  test_pass "out" "matches" "/"
else
  test_fail "out" "expected" "/"
  exit 1
fi

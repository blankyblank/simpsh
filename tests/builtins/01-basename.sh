#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

msg_run 'basename /usr/bin/foo'
out=$(../simpsh -c 'basename /usr/bin/foo')
if [ "$out" = "foo" ]; then
  test_pass "out" "matches" "foo"
else
  test_fail "out" "expected" "foo"
  exit 1
fi

msg_run 'basename foo.c .c'
out=$(../simpsh -c 'basename foo.c .c')
if [ "$out" = "foo" ]; then
  test_pass "out" "matches" "foo"
else
  test_fail "out" "expected" "foo"
  exit 1
fi

msg_run 'basename /foo/bar/baz/ (trailing slash)'
out=$(../simpsh -c 'basename /foo/bar/baz/')
if [ "$out" = "baz" ]; then
  test_pass "out" "matches" "baz"
else
  test_fail "out" "expected" "baz"
  exit 1
fi

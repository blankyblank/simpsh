#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs

tmpl=/tmp/readlink.$$

msg_run 'readlink prints symlink target'
ln -s foo "$tmpl"
out=$(../simpsh -c "readlink $tmpl")
rm -f "$tmpl"
if [ "$out" = "foo" ]; then
  test_pass "out" "matches" "foo"
else
  test_fail "out" "expected" "foo"
  exit 1
fi

msg_run 'readlink -n omits newline'
ln -s foo "$tmpl"
out=$(printf 'x'; ../simpsh -c "readlink -n $tmpl"; printf 'y')
rm -f "$tmpl"
if [ "$out" = "xfooy" ]; then
  test_pass "out" "matches" "xfooy"
else
  test_fail "out" "expected" "xfooy"
  exit 1
fi

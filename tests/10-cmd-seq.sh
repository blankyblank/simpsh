#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg_run '"printf '%s' a; printf '%s' b; printf '%s' c"'
out=$(../simpsh -c "printf '%s' a; printf '%s' b; printf '%s' c")

if [ "$out" != abc ]; then
  msg_fail "\$out: $out differs from abc"
  exit 1
else
  test_pass  "out" "matches" "abc"
fi

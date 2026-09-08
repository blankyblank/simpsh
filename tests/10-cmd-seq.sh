#!/bin/sh

[ -f ./funcs ] && . ./funcs

msg_run '; separated command test: "printf '%s' a; printf '%s' b; printf '%s' c"'
out=$(../simpsh -c "printf '%s' a; printf '%s' b; printf '%s' c")

if [ "$out" != abc ]; then
  test_fail "out" "expected" "abc"
  exit 1
else
  test_pass  "out" "matches" "abc"
fi

msg_run 'background after preceding command: sleep 30 & runs async (parse keeps &)'
out=$(timeout 10 ../simpsh -c 'sleep 30 & pid=$!; kill -0 $pid && echo bg-ok; kill $pid 2>/dev/null; wait 2>/dev/null; true')
if [ "$out" != "bg-ok" ]; then
  test_fail "out" "expected" "bg-ok"; exit 1
else
  test_pass "out" "matches" "bg-ok"
fi

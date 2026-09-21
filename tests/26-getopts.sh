#!/bin/sh
 # shellcheck disable=2016
[ -f ./funcs ] && . ./funcs

msg_run 'getopts parses flags'
out=$(../simpsh -c \
'while getopts ab: f; do
  case $f in
    a) echo opt-a;;
    b) echo opt-b $OPTARG;;
  esac
done' -- -a -b foo)
if [ "$out" = "$(printf 'opt-a\nopt-b foo')" ]; then
  test_pass "out" "matches expected" ""
else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'getopts OPTIND=1 reset reparses (optindupdt)'
out=$(../simpsh -c 'while getopts a f; do echo "A1"; done; OPTIND=1; while getopts a f; do echo "A2"; done' -- -a)
if [ "$out" = "A1
A2" ]; then test_pass "out" "matches" "A1 A2"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'getopts missing arg with :optstring -> : + OPTARG'
out=$(../simpsh -c 'while getopts ":a:" f; do case $f in a) echo "A:$OPTARG";; :) echo "MISS:$OPTARG";; ?) echo Q;; esac; done' -- -a 2>/dev/null)
if [ "$out" = "MISS:a" ]; then test_pass "out" "matches" "MISS:a"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

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

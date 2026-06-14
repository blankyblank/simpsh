#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

[ -f ./while.txt ] && rm ./while.txt

msg_run "while loop appending to file"
../simpsh -c 'i=0; while [ $i -lt 4 ]; do i=$((i+1)); echo $i >> ./while.txt; done'
out1=$(tr -d '\n' < while.txt)
if [ "$out1" -eq 1234 ]; then
  test_pass "out1" "matches" "1234"
else
  test_fail "out1" "expected" "1234"
  exit 1
fi
rm ./while.txt

msg_run "until loop appending to file"
../simpsh -c 'i=0; until [ $i -eq 4 ]; do i=$((i+1)); echo $i >> ./while.txt; done'
out2=$(tr -d '\n' < while.txt)
if [ "$out2" -eq 1234 ]; then
  test_pass "out2" "matches" "1234"
else
  test_fail "out2" "expected" "1234"
  exit 1
fi
rm ./while.txt

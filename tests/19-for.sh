#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

[ -f ./for1.txt ] && rm ./for1.txt
[ -f ./for2.txt ] && rm ./for2.txt
[ -f ./for3.txt ] && rm ./for3.txt

msg_run 'for loop test 1: for f in *; do echo $f >> ./for1.txt; done'
../simpsh -c 'for f in *; do echo $f >> ./for1.txt; done'
out1=$(cat for1.txt | grep 18-for.sh)
if [ "$out1" = "18-for.sh" ]; then
  test_pass "out1" "matches" "18-for.sh"
  rm ./for1.txt
else
  test_fail "out1" "expected" "18-for.sh"
  exit 1
fi

msg_run 'for loop test 2: for f in a b c; do echo $f >> ./for2.txt; done'
../simpsh -c 'for f in a b c; do echo $f >> ./for2.txt; done'
out2=$(tr -d '\n' <  for2.txt)
if [ "$out2" = "abc" ]; then
  test_pass "out2" "matches" "abc"
  rm ./for2.txt
else
  test_fail "out2" "expected" "abc"
  exit 1
fi

msg_run 'for loop test 3: set -- a b c; for f; do echo $f >> ./for3.txt; done'
../simpsh -c 'set -- a b c; for f; do echo $f >> ./for3.txt; done'
out3=$(tr -d '\n' <  for3.txt)
if [ "$out3" = "abc" ]; then
  test_pass "out3" "matches" "abc"
  rm ./for3.txt
else
  test_fail "out3" "expected" "abc"
  exit 1
fi


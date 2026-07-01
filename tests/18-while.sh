#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

f=./testfiles/while.txt
[ -f "$f" ] && rm $f


msg_run 'while loop test: i=0; while [ $i -lt 4 ]; do i=$((i+1)); echo $i >> $f; done'
../simpsh -c 'i=0; while [ $i -lt 4 ]; do i=$((i+1)); echo $i >> ./testfiles/while.txt; done'
out1=$(tr -d '\n' < $f)
if [ "$out1" -eq 1234 ]; then
  test_pass "out1" "matches" "1234"
else
  test_fail "out1" "expected" "1234"
  exit 1
fi
rm $f

msg_run 'until loop test: i=0; until [ $i -eq 4 ]; do i=$((i+1)); echo $i >> $f; done'
../simpsh -c 'i=0; until [ $i -eq 4 ]; do i=$((i+1)); echo $i >> ./testfiles/while.txt; done'
out2=$(tr -d '\n' < $f)
if [ "$out2" -eq 1234 ]; then
  test_pass "out2" "matches" "1234"
else
  test_fail "out2" "expected" "1234"
  exit 1
fi
rm $f

msg_run 'break test: i=0; while [ $i -lt 4 ]; do if [ $i -eq 1 ]; then break; fi i=$((i+1)); echo $i >> $f; done'
../simpsh -c 'i=0; while [ $i -lt 4 ]; do
if [ $i -eq 1 ]; then
  break
fi
i=$((i+1))
echo $i >> ./testfiles/while.txt
done'
out3=$(tr -d '\n' < $f)
if [ "$out3" -eq 1 ]; then
  test_pass "out3" "matches" "1"
else
  test_fail "out3" "expected" "1"
  exit 1
fi
rm $f

msg_run 'continue test: i=0; while [ $i -lt 4 ]; do if [ $i -eq 1 ]; then continue; fi i=$((i+1)); echo $i >> $f; done'
../simpsh -c 'i=0; while [ $i -lt 4 ]; do
i=$((i+1))
if [ $i -eq 2 ]; then
  continue
fi
echo $i >> ./testfiles/while.txt
done'
out4=$(tr -d '\n' < $f)
if [ "$out4" -eq 134 ]; then
  test_pass "out4" "matches" "134"
else
  test_fail "out4" "expected" "134"
  exit 1
fi
rm $f

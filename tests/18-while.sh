#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'while loop test: i=0; s=""; while [ $i -lt 4 ]; do i=$((i+1)); s="$s$i"; done; echo "$s"'
out1=$(../simpsh -c 'i=0; s=""; while [ $i -lt 4 ]; do i=$((i+1)); s="$s$i"; done; echo "$s"')
if [ "$out1" -eq 1234 ]; then
  test_pass "out1" "matches" "1234"
else
  test_fail "out1" "expected" "1234"
  exit 1
fi

msg_run 'until loop test: i=0; s=""; until [ $i -eq 4 ]; do i=$((i+1)); s="$s$i"; done; echo "$s"'
out2=$(../simpsh -c 'i=0; s=""; until [ $i -eq 4 ]; do i=$((i+1)); s="$s$i"; done; echo "$s"')
# out2=$(tr -d '\n' < $f)
if [ "$out2" -eq 1234 ]; then
  test_pass "out2" "matches" "1234"
else
  test_fail "out2" "expected" "1234"
  exit 1
fi

msg_run 'break test: i=0 s="" while [ $i -lt 4 ]; do if [ $i -eq 1 ]; then break fi i=$((i+1)) s="$s$i" echo $s done'
out3=$(../simpsh -c 'i=0
s=""
while [ $i -lt 4 ]; do
  if [ $i -eq 1 ]; then
    break
  fi
  i=$((i+1))
  s="$s$i"
  echo $s
done')
if [ "$out3" -eq 1 ]; then
  test_pass "out3" "matches" "1"
else
  test_fail "out3" "expected" "1"
  exit 1
fi

msg_run 'continue test: i=0; i=0 s="" while [ $i -lt 4 ]; do i=$((i+1)) if [ $i -eq 2 ]; then continue fi s="$s$i" done echo $s'
out4=$(../simpsh -c 'i=0
s=""
while [ $i -lt 4 ]; do
  i=$((i+1))
  if [ $i -eq 2 ]; then
    continue
  fi
  s="$s$i"
done
echo $s')
if [ "$out4" -eq 134 ]; then
  test_pass "out4" "matches" "134"
else
  test_fail "out4" "expected" "134"
  exit 1
fi

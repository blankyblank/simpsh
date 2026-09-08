#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs


msg_run '|| and && test 1: "false && echo true || echo false"'
out1=$(../simpsh -c "false && echo true || echo false")
if [ "$out1" != false ]; then
  test_fail "out1" "expected" "false"
  exit 1
else
  test_pass  "out1" "matches" "false"
fi

msg_run '|| and && test 2: "true && echo true || echo false"'
out2=$(../simpsh -c "true && echo true || echo false")
if [ "$out2" != true ]; then
  test_fail "out2" "expected" "true"
  exit 1
else
  test_pass  "out2" "matches" "true"
fi

msg_run '|| and && line continuation test: "true && echo true || echo false"'
out3=$(../simpsh -c 'true &&
  echo true ||
  echo false')
if [ "$out3" != true ]; then
  test_fail "out3" "expected" "true"
  exit 1
else
  test_pass  "out3" "matches" "true"
fi

msg_run 'heredoc with && same-line continuation: cat <<EOF && echo after'
out=$(../simpsh -c 'cat <<EOF && echo after
hello
EOF')
if [ "$out" != "hello
after" ]; then
  test_fail "out" "expected" "hello after"; exit 1
else
  test_pass "out" "matches" "hello after"
fi

msg_run 'heredoc with && newline continuation: cat <<EOF && (newline) body'
out=$(../simpsh -c 'cat <<EOF &&
hello
EOF
echo after')
if [ "$out" != "hello
after" ]; then
  test_fail "out" "expected" "hello after"; exit 1
else
  test_pass "out" "matches" "hello after"
fi

msg_run 'heredoc with ; continuation: cat <<EOF; echo after'
out=$(../simpsh -c 'cat <<EOF; echo after
hello
EOF')
if [ "$out" != "hello
after" ]; then
  test_fail "out" "expected" "hello after"; exit 1
else
  test_pass "out" "matches" "hello after"
fi


msg_run 'multiline group redirect with && and pipe (configure shape)'
rm -f ./testfiles/brace-group.out
../simpsh -c '{
  echo "A" &&
  echo "B" | sed "s/.*/(&)/" &&
  echo "C"
} > ./testfiles/brace-group.out'
out=$(cat ./testfiles/brace-group.out 2>/dev/null); rm -f ./testfiles/brace-group.out
if [ "$out" != "A
(B)
C" ]; then
  test_fail "out" "expected" "A (B) C"; exit 1
else
  test_pass "out" "matches" "A (B) C"
fi

#!/bin/sh
# shellcheck disable=2116
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'if test: if true; then echo true; fi'
out=$(../simpsh -c 'if true; then
  echo true
fi
  ')
if [ "$out" != "true" ]; then
  test_fail "out" "expected" "true"
  exit 1
else
  test_pass "out" "matches" "true"
fi

msg_run 'if else test: if false; then echo true; else echo false; fi'
out1=$(../simpsh -c 'if false; then
  echo true

else

  echo false
fi')
if [ "$out1" != "false" ]; then
  test_fail "out1" "expected" "false"
  exit 1
else
  test_pass "out1" "matches" "false"
fi

msg_run 'if elif else test: if false; then echo a; elif true; echo true; else echo false; fi'
out2=$(../simpsh -c 'if false; then echo a; elif true; then echo true; else echo false; fi')
if [ "$out2" != "true" ]; then
  test_fail "out2" "expected" "true"
  exit 1
else
  test_pass "out2" "matches" "true"
fi


msg_run "if with empty word condition: if ''; then echo a; else echo b; fi"
out=$(../simpsh -c "if ''; then echo a; else echo b; fi")
if [ "$out" = "b" ]; then
  test_pass "out" "matched" "b"
else
  test_fail "out" "expected" "b"
  exit 1
fi

msg_run 'if with empty quoted word: if ""; then echo a; else echo b; fi'
out=$(../simpsh -c 'if ""; then echo a; else echo b; fi')
if [ "$out" = "b" ]; then
  test_pass "out" "matched" "b"
else
  test_fail "out" "expected" "b"
  exit 1
fi

# XXX: i don't like this. never ending loop on fail if covers this already i think
# msg_run "while ''; do echo x; done; echo end"
# out=$(../simpsh -c "while ''; do echo x; done; echo end")
# if [ "$out" = "end" ]; then
#   test_pass "out" "matched" "end"
# else
#   test_fail "out" "expected" "end"
#   exit 1
# fi

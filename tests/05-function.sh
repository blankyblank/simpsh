#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run "shell function unset test: f() { echo test123; }"
out=$(../simpsh -c "f()
{ echo test123; } ; f")
if [ "$out" != "test123" ]; then
  test_fail "out" "expected" "test123"
  exit 1
else
  test_pass "out" "matches" "test123"
fi

msg_run "shell function unset test: f() { echo test123; }"
out1=$(../simpsh -c "f() { echo test123; }; unset -f f ; f" 2>&1)
if [ "$out1" != "../simpsh: f: command not found" ]; then
  test_fail "out1" "expected" "simpsh: f: command not found"
  exit 1
else
  test_pass "out1" "matches" "simpsh: f: command not found"
fi

msg_run 'local variable in function'
out=$(../simpsh -c 'f() { local x=42; echo $x; }; f; echo $x')
if [ "$out" = "42" ]; then
  test_pass "out" "matches" "42"
else
  test_fail "out" "expected" "42"
  exit 1
fi

msg_run 'return from function'
out=$(../simpsh -c 'f() { return 5; }; f; echo $?')
if [ "$out" = "5" ]; then
  test_pass "out" "matches" "5"
else
  test_fail "out" "expected" "5"
  exit 1
fi

msg_run 'function set -- contained with redirect+pipeline (shellbench shape)'
out=$(../simpsh -c 'f() { set -- "%s\n"; while IFS= read -r line; do set -- "$@" "[$line]"; done; printf "$@"; }; set -- outer1 outer2; out=$(printf "a\nb\n" | f | cat); echo "out=$out argc=$# first=$1"')
if [ "$out" = "out=[a]
[b] argc=2 first=outer1" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'function reads redirected file'
printf 'x\ny\n' >./testfiles/fn-in.txt
out=$(../simpsh -c 'f() { while IFS= read -r line; do echo "got:$line"; done; }; f < ./testfiles/fn-in.txt')
rm -f ./testfiles/fn-in.txt
if [ "$out" = "got:x
got:y" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'bare return uses last status: f() { false; return; }'
out=$(../simpsh -c 'f() { false; return; }; f; echo $?')
if [ "$out" = "1" ]; then test_pass "out" "matches" "1"; else
  test_fail "out" "expected" "1"
  exit 1
fi

msg_run 'bare return after success: f() { true; return; }'
out=$(../simpsh -c 'f() { true; return; }; f; echo $?')
if [ "$out" = "0" ]; then test_pass "out" "matches" "0"; else
  test_fail "out" "expected" "0"
  exit 1
fi

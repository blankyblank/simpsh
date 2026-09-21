#!/bin/sh

# shellcheck disable=2016
# shellcheck disable=2164

[ -f ./funcs ] && . ./funcs

here=$PWD

d=$(mktemp -d)
cd $d
msg_run 'glob * test 1: echo *'
out=$(simpsh -c 'echo *')
if [ "$out" = "*" ]; then
  test_pass "out" "matches" '*'
else
  test_fail "out" "expected" '*'
  rm -rf $d
  exit 1
fi

touch a.test b.test
msg_run 'glob * test 2: echo *.test'
out1=$(simpsh -c 'echo *.test')
if [ "$out1" = "a.test b.test" ]; then
  test_pass "out1" "matches" 'a.test b.test'
else
  test_fail "out1" "expected" 'a.test b.test'
  rm -rf $d
  exit 1
fi

msg_run 'glob ? test: echo ?.test'
out2=$(simpsh -c 'echo ?.test')
if [ "$out2" = "a.test b.test" ]; then
  test_pass "out2" "matches" 'a.test b.test'
else
  test_fail "out2" "expected" 'a.test b.test'
  rm -rf $d
  exit 1
fi
msg_run 'glob [a-z] test: echo [a-z].test'
out2=$(simpsh -c 'echo [a-z].test')
if [ "$out2" = "a.test b.test" ]; then
  test_pass "out2" "matches" 'a.test b.test'
else
  test_fail "out2" "expected" 'a.test b.test'
  rm -rf $d
  exit 1
fi
rm -rf $d

d=$(mktemp -d)
cd $d
touch a.test 1.test
msg_run 'glob [[:alpha:]] class: echo [[:alpha:]].test'
out=$(simpsh -c 'echo [[:alpha:]].test')
if [ "$out" = "a.test" ]; then test_pass "out" "matches" "a.test"; else test_fail "out" "expected" "a.test"; rm -rf $d; cd "$here"; exit 1; fi

msg_run 'glob [!a] negation: echo [!a].test'
out=$(simpsh -c 'echo [!a].test')
if [ "$out" = "1.test" ]; then test_pass "out" "matches" "1.test"; else test_fail "out" "expected" "1.test"; rm -rf $d; cd "$here"; exit 1; fi
cd "$here"; rm -rf $d

msg_run 'param trim idiom (shellbench): ${v#"${v%%[![:space:]]*}"}'
out=$(../simpsh -c 'v="  hello"; echo "[${v#"${v%%[![:space:]]*}"}]"')
if [ "$out" = "[hello]" ]; then test_pass "out" "matches" "[hello]"; else test_fail "out" "expected" "[hello]"; exit 1; fi

msg_run 'no brace expansion: echo {a,b} stays literal'
out=$(../simpsh -c 'echo {a,b}')
if [ "$out" = "{a,b}" ]; then test_pass "out" "matches" "{a,b}"; else test_fail "out" "expected" "{a,b}"; exit 1; fi

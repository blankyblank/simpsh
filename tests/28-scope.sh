#!/bin/sh
# shellcheck disable=2016,2181

[ -f ./funcs ] && . ./funcs

msg_run 'local shadows outer, restored after'
out=$(../simpsh -c 'f() { local x=inner; echo $x; }; x=outer; f; echo $x')
if [ "$out" = "inner
outer" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'local outside function fails non-zero'
../simpsh -c 'local x=1' 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'return outside function: rc=3'
../simpsh -c 'return 3' 2>/dev/null
if [ "$?" = "3" ]; then test_pass "rc" "matches" "3"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'shift in function with args'
out=$(../simpsh -c 'f() { shift; echo "$1 $#"; }; f a b c')
if [ "$out" = "b 2" ]; then test_pass "out" "matches" "b 2"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'shift too many fails non-zero'
../simpsh -c 'set -- a b; shift 5' 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'eval with redirect'
rm -f ./testfiles/ev.out
../simpsh -c 'eval "echo hi" > ./testfiles/ev.out'
out=$(cat ./testfiles/ev.out 2>/dev/null)
rm -f ./testfiles/ev.out
if [ "$out" = "hi" ]; then test_pass "out" "matches" "hi"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'umask set round-trip'
out=$(../simpsh -c 'umask 022; umask')
if [ "$out" = "0022" ]; then test_pass "out" "matches" "0022"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'umask 077 file effect'
rm -f ./testfiles/um.out
out=$(../simpsh -c 'umask 077; touch ./testfiles/um.out' && ls -l ./testfiles/um.out | awk '{print $1}')
rm -f ./testfiles/um.out
if [ "$out" = "-rw-------" ]; then test_pass "out" "matches" "-rw-------"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'readonly unset fails non-zero'
../simpsh -c 'readonly r=1; unset r' 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'export visible to children'
out=$(../simpsh -c 'export E1=v1; env' | grep E1)
if [ "$out" = "E1=v1" ]; then test_pass "out" "matches" "E1=v1"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'getopts quiet :optstring illegal -> ? with OPTARG'
out=$(../simpsh -c 'while getopts ":ab" f; do case $f in a) echo A;; b) echo B;; ?) echo "Q:$OPTARG";; esac; done' -- -a -b -z 2>/dev/null)
if [ "$out" = "A
B
Q:z" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'set -e skips exit inside && list'
out=$(../simpsh -c 'set -e; false && echo y; echo "rc=$? end"' 2>&1)
if [ "$out" = "rc=1 end" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'set -e exits inside function body (rc=1)'
../simpsh -c 'set -e; f() { false; echo infunc; }; f; echo end' >/dev/null 2>&1
if [ "$?" = "1" ]; then test_pass "rc" "matches" "1"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'set -e does not exit in if condition'
out=$(../simpsh -c 'set -e; if false; then echo y; else echo n; fi; echo end' 2>&1)
if [ "$out" = "n
end" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'local multiple vars'
out=$(../simpsh -c 'f() { local a=1 b=2; echo "$a$b"; }; f')
# => 12
msg_run 'shift 0 is a no-op success'
out=$(../simpsh -c 'set -- a b; shift 0; echo "$1 $#"')
# => a 2
msg_run 'loop with false condition exits 0'
out=$(../simpsh -c 'while false; do :; done; echo "rc=$?"')
# => rc=0
msg_run 'tilde user expands'
out=$(../simpsh -c 'echo ~root')

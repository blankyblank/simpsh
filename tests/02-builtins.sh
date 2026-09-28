#!/bin/sh
# shellcheck disable=2015,2181,2016

[ -f ./funcs ] && . ./funcs

msg_run 'echo test: "echo Test test!"'
out=$(../simpsh -c "echo Test test!")

if [ "$out" != "Test test!" ]; then
  test_fail "out" "differs from" "Test test!"
  exit 1
else
  test_pass "out" "matches" "Test test!"
fi

msg_run 'read builtin'
out=$(printf 'hello world\n' | ../simpsh -c 'read line; echo $line')
if [ "$out" = "hello world" ]; then
  test_pass "out" "matches" "hello world"
else
  test_fail "out" "expected" "hello world"
  exit 1
fi

msg_run 'printf builtin'
out=$(../simpsh -c 'printf "%d %s" 42 world')
if [ "$out" = "42 world" ]; then
  test_pass "out" "matches" "42 world"
else
  test_fail "out" "expected" "42 world"
  exit 1
fi

msg_run 'cd/pwd test: "cd /tmp ; pwd"'
out=$(../simpsh -c "cd /tmp ; pwd")

if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"
  exit 1
else
  test_pass "out" "matches" "/tmp"
fi

msg_run 'exit builtin test 1: "exit 0"; echo $?'
zero=$(../simpsh -c "exit 0"; echo $?)
if [ "$zero" -eq 0 ]; then
  msg_pass "\$zero=$zero correct exit status"
else
  msg_fail "exit 0 produced incorrect value"
  exit 1
fi

msg_run 'exit builtin test 2: exit 5" ; echo $?'
five=$(../simpsh -c "exit 5" ; echo $?)
if [ "$five" -eq 5 ]; then
  msg_pass "\$five=$five correct exit status"
else
  msg_fail "exit 5 produced incorrect value"
  exit 1
fi

msg_run 'eval'
out=$(../simpsh -c 'x=hello; eval "echo \$x"')
if [ "$out" = "hello" ]; then
  test_pass "out" "matches" "hello"
else
  test_fail "out" "expected" "hello"
  exit 1
fi

msg_run 'exec simple command'
out=$(../simpsh -c 'exec echo hi')
if [ "$out" = "hi" ]; then
  test_pass "out" "matches" "hi"
else
  test_fail "out" "expected" "hi"
  exit 1
fi

msg_run 'type builtin'
out=$(../simpsh -c 'type echo')
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected output" ""
  exit 1
fi

msg_run 'command -v'
out=$(../simpsh -c 'command -v echo')
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected output" ""
  exit 1
fi

# === kill ===
msg_run 'kill -l lists signal names'
out=$(../simpsh -c 'kill -l')
if [ -n "$out" ] && echo "$out" | grep -q "INT"; then
  msg_pass "contains INT"
else
  test_fail "out" "expected signal list" ""
  exit 1
fi

msg_run 'kill -0 checks process existence'
../simpsh -c 'kill -0 $$' 2>/dev/null
rc=$?
if [ $rc -eq 0 ]; then
  test_pass "rc" "0 (process exists)" ""
else
  test_fail "rc" "expected 0" "$rc"
  exit 1
fi

# === hash ===
msg_run 'hash displays hash table'
out=$(../simpsh -c 'hash')
if [ $? -eq 0 ]; then
  test_pass "hash" "exits 0" ""
else
  test_fail "hash" "expected success" ""
  exit 1
fi

msg_run 'hash -r clears hash table'
../simpsh -c 'hash -r'
if [ $? -eq 0 ]; then
  test_pass "hash -r" "exits 0" ""
else
  test_fail "hash -r" "expected success" ""
  exit 1
fi

# === umask ===
msg_run 'umask prints current mask'
out=$(../simpsh -c 'umask')
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected umask" ""
  exit 1
fi

# === ulimit ===
msg_run 'ulimit -n prints fd limit'
out=$(../simpsh -c 'ulimit -n')
if [ "$out" -ge 0 ] 2>/dev/null; then
  test_pass "out" "valid number" ""
else
  test_fail "out" "expected numeric" "$out"
  exit 1
fi

msg_run 'ulimit -n N (no -H/-S) sets both soft and hard'
out=$(../simpsh -c '
  cur=$(ulimit -n)
  ulimit -n "$cur" || exit 1
  [ "$(ulimit -n)" = "$cur" ] && [ "$(ulimit -Hn)" = "$cur" ]
' 2>&1)
if [ -z "$out" ]; then
  test_pass "ulimit" "sets soft+hard" ""
else
  test_fail "ulimit" "expected no error and hard == soft" "$out"
  exit 1
fi

msg_run 'ulimit -n N raises hard above old hard (root)'
if [ "$(id -u)" -eq 0 ]; then
  out=$(../simpsh -c '
    ulimit -n 131072 || exit 1
    [ "$(ulimit -n)" = 131072 ] && [ "$(ulimit -Hn)" = 131072 ]
  ' 2>&1)
  if [ -z "$out" ]; then
    test_pass "ulimit" "raised soft+hard to 131072" ""
  else
    test_fail "ulimit" "expected 131072 soft+hard" "$out"
    exit 1
  fi
else
  test_pass "ulimit root-case" "skipped (not root)" ""
fi

msg_run 'cd . keeps logical path: cd /tmp; cd .; pwd'
out=$(../simpsh -c 'cd /tmp; cd .; pwd')
if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"; exit 1
else
  test_pass "out" "matches" "/tmp"
fi

msg_run 'dot-slash normalization: cd /tmp/./ && pwd'
out=$(../simpsh -c 'cd /tmp/./ && pwd')
if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"; exit 1
else
  test_pass "out" "matches" "/tmp"
fi

msg_run 'cd -P physical: cd -P /tmp && pwd'
out=$(../simpsh -c 'cd -P /tmp && pwd')
if [ "$out" != "/tmp" ]; then
  test_fail "out" "expected" "/tmp"; exit 1
else
  test_pass "out" "matches" "/tmp"
fi

msg_run 'read -r keeps backslash'
out=$(printf 'a\\b\n' | ../simpsh -c 'read -r line; echo "$line"')
if [ "$out" = "$(printf 'a\b')" ]; then test_pass "out" "matches backspace form"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'read without -r strips backslash'
out=$(printf 'a\\b\n' | ../simpsh -c 'read line; echo "$line"')
if [ "$out" = "ab" ]; then test_pass "out" "matches" "ab"; else
  test_fail "out" "expected" "ab"
  exit 1
fi

msg_run 'read IFS split'
out=$(echo "a:b:c" | ../simpsh -c 'IFS=: read a b c; echo "$a|$b|$c"')
if [ "$out" = "a|b|c" ]; then test_pass "out" "matches" "a|b|c"; else
  test_fail "out" "expected" "a|b|c"
  exit 1
fi

msg_run 'read extra words go to last var'
out=$(echo "a b c d" | ../simpsh -c 'read a b; echo "$a|$b"')
if [ "$out" = "a|b c d" ]; then test_pass "out" "matches" "a|b c d"; else
  test_fail "out" "expected" "a|b c d"
  exit 1
fi

msg_run 'read EOF clears var (shellbench spiral)'
out=$(printf 'x\n' | ../simpsh -c 'read line; echo "[$line]"; read line; echo "rc=$? line=[$line]"')
if [ "$out" = "[x]
rc=1 line=[]" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'read no-trailing-newline idiom delivers last line once'
out=$(printf 'last' | ../simpsh -c 'read line || [ -n "$line" ]; echo "got:$line"')
if [ "$out" = "got:last" ]; then test_pass "out" "matches" "got:last"; else
  test_fail "out" "expected" "got:last"
  exit 1
fi

msg_run 'while read counts lines incl unterminated last'
out=$(printf 'a\nb\nc' | ../simpsh -c 'n=0; while IFS= read -r line || [ -n "$line" ]; do n=$((n+1)); done; echo $n')
if [ "$out" = "3" ]; then test_pass "out" "matches" "3"; else
  test_fail "out" "expected" "3"
  exit 1
fi

msg_run 'read empty input clears var, status 1'
out=$(printf '' | ../simpsh -c 'line=stale; read line; echo "rc=$? line=[$line]"')
if [ "$out" = "rc=1 line=[]" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'kill %1 job spec kills bg sleep (rc=143)'
out=$(../simpsh -c 'sleep 0.2 & kill %1; wait %1; echo "rc=$?"' 2>/dev/null)
if [ "$out" = "rc=143" ]; then test_pass "out" "matches" "rc=143"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'wait bad pid: rc=127'
../simpsh -c 'wait 999999' 2>/dev/null
if [ "$?" = "127" ]; then test_pass "rc" "matches" "127"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'read without varname fails non-zero'
../simpsh -c 'read' </dev/null 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'cd to file fails non-zero'
../simpsh -c 'cd /etc/passwd' 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'trap bogus signal fails non-zero'
../simpsh -c 'trap "echo x" BOGUS' 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'export bare lists (non-empty)'
out=$(../simpsh -c 'export' 2>/dev/null)
if [ -n "$out" ]; then test_pass "out" "non-empty" ""; else
  test_fail "out" "expected output" ""
  exit 1
fi

msg_run 'times exits 0'
../simpsh -c 'times' >/dev/null 2>&1
if [ "$?" = "0" ]; then test_pass "rc" "matches" "0"; else
  test_fail "rc" "unexpected" "$?"
  exit 1
fi

msg_run 'umask -S after umask 022'
out=$(../simpsh -c 'umask 022; umask -S')
if [ "$out" = "u=rwx,g=rx,o=rx" ]; then test_pass "out" "matches" ""; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'cd - returns to OLDPWD'
out=$(../simpsh -c 'cd /tmp; cd /; cd - >/dev/null; pwd')
if [ "$out" = "/tmp" ]; then test_pass "out" "matches" "/tmp"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'kill %1 job spec (rc=143)'
out=$(../simpsh -c 'sleep 0.2 & kill %1; wait %1; echo "rc=$?"' 2>/dev/null)
if [ "$out" = "rc=143" ]; then test_pass "out" "matches" "rc=143"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'read -p prompts on stderr, sets var'
out=$(printf 'x\n' | ../simpsh -c 'read -p "P:" v 2>&1 >/dev/null; echo "v=$v"')
if [ "$out" = "P:v=x" ]; then test_pass "out" "matches" "P:v=x"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'umask symbolic auto-detect: umask u=rwx; umask'
out=$(../simpsh -c 'umask u=rwx; umask')
if [ "$out" = "0022" ]; then test_pass "out" "matches" "0022"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'umask symbolic a=rx sets 0222'
out=$(../simpsh -c 'umask a=rx; umask')
if [ "$out" = "0222" ]; then test_pass "out" "matches" "0222"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'umask symbolic multi-clause: u=rwx,go=rx'
out=$(../simpsh -c 'umask u=rwx,go=rx; umask')
if [ "$out" = "0022" ]; then test_pass "out" "matches" "0022"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

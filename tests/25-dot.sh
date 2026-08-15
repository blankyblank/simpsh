#!/bin/sh

[ -f ./funcs ] && . ./funcs

cat > ./testfiles/source.sh <<'SOURCEEOF'
echo "sourced: $1 $2"
SOURCEEOF

msg_run 'dot (source) builtin'
out=$(../simpsh -c '. ./testfiles/source.sh a b')
if [ "$out" = "sourced: a b" ]; then
  test_pass "out" "matches" "sourced: a b"
else
  test_fail "out" "expected" "sourced: a b"
  exit 1
fi

mkdir -p /tmp/simpsh_dot
printf 'set -- x y\n' > /tmp/simpsh_dot/setargs.sh
printf 'set -- nscd\necho "hook: $@"\n' > /tmp/simpsh_dot/hook_a.sh
printf 'echo "sub: $@"\n' > /tmp/simpsh_dot/hook_b.sh
printf '. /tmp/simpsh_dot/hook_a.sh\n. /tmp/simpsh_dot/hook_b.sh\nexit 0\n' > /tmp/simpsh_dot/upd_hook.sh
printf 'return 5\n' > /tmp/simpsh_dot/ret.sh

msg_run "dot crash pattern: ( set -- up wg0; . upd_hook.sh ); echo rc=\$?"
out=$(../simpsh -c '( set -- up wg0; . /tmp/simpsh_dot/upd_hook.sh ); echo rc=$?')
if [ "$out" != "hook: nscd
sub: nscd
rc=0" ]; then
  test_fail "out" "expected" "hook: nscd / sub: nscd / rc=0"; exit 1
else
  test_pass "out" "matches" "hook: nscd / sub: nscd / rc=0"
fi

msg_run "dot set -- persists: . setargs.sh; echo \$@"
out=$(../simpsh -c '. /tmp/simpsh_dot/setargs.sh; echo "$@"')
if [ "$out" != "x y" ]; then
  test_fail "out" "expected" "x y"; exit 1
else
  test_pass "out" "matches" "x y"
fi

msg_run "dot return status: . ret.sh; echo \$?  (was rc=0 bug)"
out=$(../simpsh -c '. /tmp/simpsh_dot/ret.sh; echo $?')
if [ "$out" != "5" ]; then
  test_fail "out" "expected" "5"; exit 1
else
  test_pass "out" "matches" "5"
fi

rm -rf /tmp/simpsh_dot

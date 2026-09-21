#!/bin/sh
# shellcheck disable=2016,2181

[ -f ./funcs ] && . ./funcs

ulimit -c 0

msg_run 'trap INT: kill -INT $$ from subshell'
out=$(../simpsh -c 'trap "echo trapped" INT; kill -INT $$; echo alive')
if [ "$out" = "alive
trapped" ]; then
  test_pass "out" "matches" "trapped"
else
  test_fail "out" "expected" "trapped"
  exit 1
fi

msg_run 'trap EXIT: exit handler fires'
out=$(../simpsh -c 'trap "echo bye" EXIT; echo -n "hello "')
if [ "$out" = "hello bye" ]; then
  test_pass "out" "matches" "hello bye"
else
  test_fail "out" "expected" "hello bye"
  exit 1
fi

msg_run 'trap \"\" QUIT: ignore SIGQUIT'
out=$(../simpsh -c 'trap "" QUIT; kill -QUIT $$; echo survived')
if [ "$out" = "survived" ]; then
  test_pass "out" "matches" "survived"
else
  test_fail "out" "expected" "survived"
  exit 1
fi

msg_run 'trap - QUIT: reset to default, shell dies'
../simpsh -c 'trap "" QUIT; trap - QUIT; kill -QUIT $$' 2>/dev/null
rc=$?
if [ "$rc" -ge 128 ]; then
  test_pass "rc" "matches" ">=128"
else
  test_fail "rc" "expected" ">=128"
  exit 1
fi

msg_run 'trap QUIT: reset to default (no action)'
../simpsh -c 'trap "" QUIT; trap QUIT; kill -QUIT $$' 2>/dev/null
rc=$?
if [ "$rc" -ge 128 ]; then
  test_pass "rc" "matches" ">=128"
else
  test_fail "rc" "expected" ">=128"
  exit 1
fi

msg_run 'trap \"echo x\" INT TERM: multiple signals'
out=$(../simpsh -c 'trap "echo caught" INT TERM; kill -INT $$; echo alive')
if [ "$out" = "alive
caught" ]; then
  test_pass "out" "matches" "caught"
else
  test_fail "out" "expected" "caught"
  exit 1
fi

msg_run 'trap with no args: lists set traps'
out=$(../simpsh -c 'trap "echo x" INT; trap')
if echo "$out" | grep -q INT; then
  test_pass "out" "contains" "INT"
else
  test_fail "out" "expected to contain" "INT"
  exit 1
fi

msg_run 'EXIT trap preserves non-zero exit'
../simpsh -c 'trap "true" EXIT; false' >/dev/null 2>&1
rc=$?
if [ "$rc" = "1" ]; then
  test_pass "rc" "matches" "1"
else
  test_fail "rc" "expected" "1"
  exit 1
fi

msg_run 'trap reset in subshell does not leak to parent'
out=$(../simpsh -c 'trap "" TERM; (trap - TERM); kill -TERM $$; echo survived')
if [ "$out" = "survived" ]; then
  test_pass "out" "matches" "survived"
else
  test_fail "out" "expected" "survived"
  exit 1
fi

msg_run 'trap \"\" PIPE: set (no crash)'
out=$(../simpsh -c 'trap "" PIPE; echo ok')
if [ "$out" = "ok" ]; then
  test_pass "out" "matches" "ok"
else
  test_fail "out" "expected" "ok"
  exit 1
fi

msg_run 're-trap after ignore: trap "" HUP; trap "echo hi" HUP'
out=$(../simpsh -c 'trap "" HUP; trap "echo hi" HUP; kill -HUP $$; echo done')
if [ "$out" = "done
hi" ]; then test_pass "out" "matches" "done hi"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'ignore preserved in subshell listing + survives HUP'
out=$(../simpsh -c 'trap "" HUP; (trap | grep -c HUP); kill -HUP $$; echo survived')
if [ "$out" = "1
survived" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'ignore preserved across exec: trap "" HUP; sh -c kill'
out=$(../simpsh -c 'trap "" HUP; ../simpsh -c "kill -HUP \$\$; echo child-survived"')
if [ "$out" = "child-survived" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'caught trap reset in exec child (dies, rc>=128)'
../simpsh -c 'trap "echo caught" HUP; ../simpsh -c "kill -HUP \$\$"' 2>/dev/null
rc=$?
if [ "$rc" -ge 128 ]; then test_pass "rc" "died on HUP" "$rc"; else
  test_fail "rc" "expected >=128" "$rc"
  exit 1
fi

msg_run 'kill -s TERM kills bg sleep (rc=143)'
out=$(../simpsh -c 'sleep 0.2 & p=$!; kill -s TERM $p; wait $p; echo "rc=$?"' 2>/dev/null)
if [ "$out" = "rc=143" ]; then test_pass "out" "matches" "rc=143"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'kill bogus signal fails non-zero'
../simpsh -c 'kill -s BOGUS $$' 2>/dev/null
if [ "$?" -ne 0 ]; then test_pass "rc" "non-zero" ""; else
  test_fail "rc" "expected failure" ""
  exit 1
fi

msg_run 'kill bad pgid: No such process, rc=1'
out=$(../simpsh -c 'kill -HUP -99999999; echo "rc=$?"' 2>&1)
if [ "$out" = "../simpsh: kill: -99999999: No such process
rc=1" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'wait all bg jobs'
out=$(../simpsh -c 'sleep 0.2 & sleep 0.2 & wait; echo "all=$?"')
if [ "$out" = "all=0" ]; then test_pass "out" "matches" "all=0"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'trapped HUP during wait still delivers (order-agnostic)'
out=$(../simpsh -c 'trap "echo trapped" HUP; sleep 0.2 & kill -HUP $$; wait; echo done')
if echo "$out" | grep -q done && echo "$out" | grep -q trapped; then test_pass "out" "has done+trapped" ""; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run 'sh -c stays in parent pgid (yazi/nvim SIGHUP regression)'
out=$(../simpsh -c 'p=$(ps -o pgid= -p $$ | tr -d " "); c=$(../simpsh -c "ps -o pgid= -p \$\$ | tr -d \" \""); [ "$p" = "$c" ] && echo same || echo "DIFF $p $c"')
if [ "$out" = "same" ]; then test_pass "out" "pgid inherited" ""; else
  test_fail "out" "pgid changed" "$out"
  exit 1
fi

msg_run 'HUP orchestration miniature (bench/stopper shape)'
cat >./testfiles/hup-mini.sh <<'MINIEOF'
trap '' HUP
bench() {
  MAIN_PID=$$
  ready=0
  trap 'ready=$(($ready + 1))' HUP
  worker() { trap 'wready=1' HUP; kill -HUP $MAIN_PID; until [ "$wready" ]; do dummy=; done; echo WORKER-DONE; }
  stopper() { trap 'sready=1' HUP; kill -HUP $MAIN_PID; until [ "$sready" ]; do dummy=; done; echo STOPPER-DONE; }
  worker &
  wpid=$!
  stopper &
  spid=$!
  while [ "$ready" -lt 2 ]; do dummy=; done
  echo "ready=$ready"
  kill -HUP $wpid $spid
  wait
  echo bench-done
}
bench
echo main-done
MINIEOF
out=$(timeout 10 ../simpsh ./testfiles/hup-mini.sh 2>&1)
rm -f ./testfiles/hup-mini.sh
for line in WORKER-DONE STOPPER-DONE "ready=2" bench-done main-done; do
  echo "$out" | grep -q "$line" || {
    test_fail "out" "missing" "$line"
    exit 1
  }
done
test_pass "out" "has all orchestration lines" ""

msg_run 'trap TERM round-trip (deferred delivery like INT)'
out=$(../simpsh -c 'trap "echo t" TERM; kill -TERM $$; echo alive')
if [ "$out" = "alive
t" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

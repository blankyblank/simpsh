#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

cat >./testfiles/lineno.sh <<'LINENOEOF'
echo "top=$LINENO"
f() { echo "func=$LINENO"; }
f
LINENOEOF
msg_run '$LINENO in script and function'
out=$(../simpsh ./testfiles/lineno.sh)
rm -f ./testfiles/lineno.sh
if [ "$out" = "top=1
func=1" ]; then test_pass "out" "matches"; else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

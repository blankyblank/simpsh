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

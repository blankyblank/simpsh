#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

if [ -x ./test ]; then
  msg "Running C unit tests..."
  ./test || { msg_fail "C unit tests"; exit 1;}
fi

msg "Running shell tests..."
for f in [0-9]-*.sh; do
  [ ! -x ./$f ] && continue
  if ./$f; then
    msg_pass "$f"
  else
    msg_fail "$f"
    exit 1
  fi
done

msg_pass "all tests"

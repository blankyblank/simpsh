#!/bin/sh
# shellcheck disable=2015

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg "Running builtin tests..."
for f in builtins/[0-9][0-9]-*.sh; do
  [ ! -x "$f" ] && continue
  name=${f##*/}
  name=${name%.sh}
  name=${name#*-}
  flag=$(printf 'ENABLE_%s' "$name" | tr a-z A-Z)
  if grep -q "^#define $flag 1$" ../config.h; then
    msg "$f..."
    if ! "./$f"; then
      msg_fail "$f"
      exit 1
    fi
  else
    msg "$name:not enabled, skipping..."
  fi
done
msg_pass "all extra builtins"

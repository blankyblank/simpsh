#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg 'running ../simpsh -c "false && echo true || echo false"...'
out1=$(../simpsh -c "false && echo true || echo false")
msg 'running ../simpsh -c "true && echo true || echo false"...'
out2=$(../simpsh -c "true && echo true || echo false")

msg "getting results..."
if [ "$out1" != false ]; then
  msg_fail "result should have been false"
  exit 1
fi
if [ "$out2" != true ]; then
  msg_fail "result should have been true"
  exit 1
fi

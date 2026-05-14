#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg 'running ../simpsh -c "cd /tmp ; pwd"...'
out=$(../simpsh -c "cd /tmp ; pwd")

if [ "$out" != "/tmp" ]; then
  msg_fail "output differs"
  exit 1
fi

exit 0

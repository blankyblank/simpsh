#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

msg 'running ../simpsh -c "TMPDIR=/tmp cd; pwd" >/dev/null...'
../simpsh -c "TMPDIR=/tmp cd; pwd" >/dev/null || { msg_fail "precmd builtin failed"; exit 1; }

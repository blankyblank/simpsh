#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

../simpsh -c "TMPDIR=/tmp cd; pwd" || { msg_fail "precmd builtin failed"; exit 1; }

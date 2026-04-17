#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

out=$(../simpsh -c "testvar=1 env" | grep testvar)

[ $out = testvar=1 ] || msg_fail "output differs" ; exit 1

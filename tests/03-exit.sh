#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2181
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

../simpsh -c "exit 0"
[ $? -eq 0 ] || { msg_fail "exit 0 produced incorrect value"; exit 1;}
../simpsh -c "exit 5"
[ $? -eq 0 ] || { msg_fail "exit 5 produced incorrect value"; exit 1;}

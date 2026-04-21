#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2016
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

out=$(echo 'foo=bar; echo $foo' | ../simpsh )
[ "$out" = "bar" ] || { msg_fail "outputs differ"; exit 1;}

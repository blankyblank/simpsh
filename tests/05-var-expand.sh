#!/bin/sh
# shellcheck disable=2015
set -e

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

out=$(../simpsh -c "foo=bar; echo $foo")
[ $out = "bar" ] || { msg_fail "outputs differ"; exit 1;}

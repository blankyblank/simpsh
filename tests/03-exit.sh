#!/bin/sh
# shellcheck disable=2015
# shellcheck disable=2181

[ -f ./funcs ] && . ./funcs || { echo "no ./funcs file"; exit 1; }

zero=$(../simpsh -c "exit 0"; echo $?)
[ "$zero" -eq 0 ] || { msg_fail "exit 0 produced incorrect value"; exit 1;}
five=$(../simpsh -c "exit 5" ; echo $?)
[ "$five" -eq 5 ] || { msg_fail "exit 5 produced incorrect value"; exit 1;}

exit 0

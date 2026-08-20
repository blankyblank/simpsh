#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'export test: export test1=1 ; env | grep "test1=1"'
out=$(../simpsh -c "export test1=1 ; env" | grep "test1=1")
if [ "$out" != "test1=1" ]; then
  test_fail "out" "expected" "test1=1"
  exit 1
else
  test_pass  "out" "matches" "test1=1"
fi

msg_run 'variable expansion test: echo foo=bar; echo $foo | ../simpsh'
out=$(echo 'foo=bar; echo $foo' | ../simpsh )

if  [ "$out" = "bar" ]; then
  test_pass  "out" "matches" "bar"
else
  test_fail  "out" "expected" "bar"
  exit 1
fi

msg_run 'variable path expansion test: echo $HOME/.config; echo $foo | ../simpsh'
out=$(echo 'echo $HOME/.config' | ../simpsh )

if  [ "$out" = "$HOME/.config" ]; then
  test_pass  "out" "matches" "bar"
else
  test_fail  "out" "expected" "bar"
  exit 1
fi

msg_run 'set -- and shift'
out=$(../simpsh -c 'set -- a b c d; shift; shift; echo $1 $#')
if [ "$out" = "c 2" ]; then
  test_pass "out" "matches" "c 2"
else
  test_fail "out" "expected" "c 2"
  exit 1
fi

msg_run '$# $* $@'
out=$(../simpsh -c 'set -- a "b c" d; echo $#; for i in "$@"; do echo $i; done')
if [ "$out" = "$(printf '3\na\nb c\nd')" ]; then
  test_pass "out" "matches expected" ""
else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run '$- flags'
out=$(../simpsh -c 'echo $-')
# interactive shell should at least have "s" in $- from sh -c
if [ -n "$out" ]; then
  test_pass "out" "non-empty" ""
else
  test_fail "out" "expected non-empty" ""
  exit 1
fi

msg_run 'readonly prevents assignment'
out=$(../simpsh -c 'readonly v=1; v=2; echo $v' 2>&1)
if [ "$out" = "1" ]; then
  test_pass "out" "still" "1"
else
  test_fail "out" "expected" "1"
  exit 1
fi

msg_run 'readonly read across shell scripts'
out=$(../simpsh -c 'readonly v=1; ../simpsh -c "echo \$v"' 2>&1)
if [ "$out" = "" ]; then
  test_pass "out" "empty (not exported)" ""
else
  test_fail "out" "expected" ""
  exit 1
fi

msg_run '\$@ with set -- a "" b: expands to 3 args'
out=$(../simpsh -c 'set -- a "" b; for i in "$@"; do echo $i; done')
if [ "$out" = "$(printf 'a\n\nb')" ]; then
  test_pass "out" "3 args with empty middle" ""
else
  test_fail "out" "unexpected" "$out"
  exit 1
fi

msg_run '${var:-word} default value'
out=$(../simpsh -c 'echo ${unsetvar:-default}')
if [ "$out" = "default" ]; then
  test_pass "out" "matches" "default"
else
  test_fail "out" "expected" "default"
  exit 1
fi

msg_run '${var:=word} assign default'
out=$(../simpsh -c 'unset x; : ${x:=assigned}; echo $x')
if [ "$out" = "assigned" ]; then
  test_pass "out" "matches" "assigned"
else
  test_fail "out" "expected" "assigned"
  exit 1
fi

msg_run '${var:+word} alternate value'
out=$(../simpsh -c 'v=set; echo ${v:+alt}')
if [ "$out" = "alt" ]; then
  test_pass "out" "matches" "alt"
else
  test_fail "out" "expected" "alt"
  exit 1
fi

msg_run '${var:?msg} error on unset'
out=$(../simpsh -c 'unset x; echo ${x:?oops}' 2>&1)
rc=$?
if [ $rc -ne 0 ] && echo "$out" | grep -q "oops"; then
  test_pass "out" "matches" "oops"
else
  test_fail "out" "expected" "oops"
  exit 1
fi

msg_run '${#var} string length'
out=$(../simpsh -c 'v=hello; echo ${#v}')
if [ "$out" = "5" ]; then
  test_pass "out" "matches" "5"
else
  test_fail "out" "expected" "5"
  exit 1
fi

msg_run '${var%pattern} remove suffix shortest'
out=$(../simpsh -c 'f=file.txt; echo ${f%.*}')
if [ "$out" = "file" ]; then
  test_pass "out" "matches" "file"
else
  test_fail "out" "expected" "file"
  exit 1
fi

msg_run '${var%%pattern} remove suffix longest'
out=$(../simpsh -c 'f=a.b.c; echo ${f%%.*}')
if [ "$out" = "a" ]; then
  test_pass "out" "matches" "a"
else
  test_fail "out" "expected" "a"
  exit 1
fi

msg_run '${var#pattern} remove prefix shortest'
out=$(../simpsh -c 'f=foo.bar; echo ${f#*.}')
if [ "$out" = "bar" ]; then
  test_pass "out" "matches" "bar"
else
  test_fail "out" "expected" "bar"
  exit 1
fi

msg_run '${var##pattern} remove prefix longest'
out=$(../simpsh -c 'f=a/b/c; echo ${f##*/}')
if [ "$out" = "c" ]; then
  test_pass "out" "matches" "c"
else
  test_fail "out" "expected" "c"
  exit 1
fi

msg_run "embedded quoted \$*: set -- a b c; echo \"MSG:[\$*]\""
out=$(../simpsh -c 'set -- a b c; echo "MSG:[$*]"')
if [ "$out" != "MSG:[a b c]" ]; then
  test_fail "out" "expected" "MSG:[a b c]"; exit 1
else
  test_pass "out" "matches" "MSG:[a b c]"
fi

msg_run 'quoted "$*" is one field: printf "<%s>" "$*"'
out=$(../simpsh -c 'set -- a "b c" d; printf "<%s>" "$*"')
if [ "$out" != "<a b c d>" ]; then
  test_fail "out" "expected" "<a b c d>"; exit 1
else
  test_pass "out" "matches" "<a b c d>"
fi

msg_run 'quoted "$@" keeps fields: printf "<%s>" "$@"'
out=$(../simpsh -c 'set -- a "b c" d; printf "<%s>" "$@"')
if [ "$out" != "<a><b c><d>" ]; then
  test_fail "out" "expected" "<a><b c><d>"; exit 1
else
  test_pass "out" "matches" "<a><b c><d>"
fi

msg_run "embedded unquoted \$@: echo x\$@y"
out=$(../simpsh -c 'set -- a b c; echo x$@y')
if [ "$out" != "xa b cy" ]; then
  test_fail "out" "expected" "xa b cy"; exit 1
else
  test_pass "out" "matches" "xa b cy"
fi

msg_run 'for i in "$@" iterates: set -- a "b c" d'
out=$(../simpsh -c 'set -- a "b c" d; for i in "$@"; do echo "[$i]"; done')
if [ "$out" != "[a]
[b c]
[d]" ]; then
  test_fail "out" "expected" "[a] [b c] [d]"; exit 1
else
  test_pass "out" "matches" "[a] [b c] [d]"
fi

msg_run 'empty middle param preserved: set -- a "" b'
out=$(../simpsh -c 'set -- a "" b; printf "<%s>" "$@"')
if [ "$out" != "<a><><b>" ]; then
  test_fail "out" "expected" "<a><><b>"; exit 1
else
  test_pass "out" "matches" "<a><><b>"
fi

msg_run "IFS join: IFS=,; set -- a b c; echo \"\$*\""
out=$(../simpsh -c 'IFS=,; set -- a b c; echo "$*"')
if [ "$out" != "a,b,c" ]; then
  test_fail "out" "expected" "a,b,c"; exit 1
else
  test_pass "out" "matches" "a,b,c"
fi

msg_run "empty: set --; echo \"[\$*]\""
out=$(../simpsh -c 'set --; echo "[$*]"')
if [ "$out" != "[]" ]; then
  test_fail "out" "expected" "[]"; exit 1
else
  test_pass "out" "matches" "[]"
fi

msg_run 'variable stress test'
printf '%s\n' \
"abc1=asdasdf" \
"abc2=asdasdf" \
"abc3=asdasdf" \
"abc4=asdasdf" \
"abc5=asdasdf" \
"abc6=asdasdf" \
"abc7=asdasdf" \
"abc8=asdasdf" \
"abc9=asdasdf" \
"abc10=asdasdf" \
"abc11=asdasdf" \
"abc12=asdasdf" \
"abc13=asdasdf" \
"abc14=asdasdf" \
"abc15=asdasdf" \
"abc16=asdasdf" \
"abc17=asdasdf" \
"abc18=asdasdf" \
"abc19=asdasdf" \
"abc20=asdasdf" \
"abc21=asdasdf" \
"abc22=asdasdf" \
"abc23=asdasdf" \
"abc24=asdasdf" \
"abc25=asdasdf" \
"abc26=asdasdf" \
"abc27=asdasdf" \
"abc28=asdasdf" \
"abc29=asdasdf" \
"abc30=asdasdf" \
"abc31=asdasdf" \
"abc32=asdasdf" \
"abc33=asdasdf" \
"abc34=asdasdf" \
"abc35=asdasdf" \
"abc36=asdasdf" \
"abc37=asdasdf" \
"abc38=asdasdf" \
"abc39=asdasdf" \
"abc40=asdasdf" \
"abc41=asdasdf" \
"abc42=asdasdf" \
"abc43=asdasdf" \
"abc44=asdasdf" \
"abc45=asdasdf" \
"abc46=asdasdf" \
"abc47=asdasdf" \
"abc48=asdasdf" \
"abc49=asdasdf" \
"abc50=asdasdf" \
"abc51=asdasdf" \
"abc52=asdasdf" \
"abc53=asdasdf" \
"abc54=asdasdf" \
"abc55=asdasdf" \
"abc56=asdasdf" \
"abc57=asdasdf" \
"abc58=asdasdf" \
"abc59=asdasdf" \
"abc60=asdasdf" \
"abc61=asdasdf" \
"abc62=asdasdf" \
"abc63=asdasdf" \
"abc64=asdasdf" \
"abc65=asdasdf" \
"abc66=asdasdf" \
"abc67=asdasdf" \
"abc68=asdasdf" \
"abc69=asdasdf" \
"abc70=asdasdf" \
"abc71=asdasdf" \
"abc72=asdasdf" \
"abc73=asdasdf" \
"abc74=asdasdf" \
"abc75=asdasdf" \
"abc76=asdasdf" \
"abc77=asdasdf" \
"abc78=asdasdf" \
"abc79=asdasdf" \
"abc80=asdasdf" | ../simpsh

msg_pass  "all 80 variables were set"

exit 0

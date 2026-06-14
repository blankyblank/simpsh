#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run 'export test1=1 ; env | grep "test1=1"'
out=$(../simpsh -c "export test1=1 ; env" | grep "test1=1")

if [ "$out" != "test1=1" ]; then
  test_fail "out" "expected" "test1=1"
  exit 1
else
  test_pass  "out" "matches" "test1=1"
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

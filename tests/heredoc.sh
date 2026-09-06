#!/home/blank/dev/simpsh/simpsh

var=word # comment
case "$var" in #inline comment
  word) #inline comment3
    echo word #inline comment2
    ;; #cmmt
  *) #cmmt
    echo not-word #cmmt
    ;; #cmmt
esac  #cmmt

cat >/tmp/o1 <<_ACEOF
hello
_ACEOF
wc -c </tmp/o1

cat >/tmp/o2 <<_ACEOF
hello
_ACEOF
wc -c </tmp/o2

cat >/tmp/o3 <<_ACEOF
hello
_ACEOF
wc -c </tmp/o3

cat >/tmp/o4 <<_ACEOF
A!$A$ac_delim
_ACEOF
wc -c </tmp/o4

x=$(cat <<_ACEOF
hello
_ACEOF
)
printf '%s\n' "$x" | wc -c

y=$(cat <<_ACEOF
A!$A$ac_delim
_ACEOF
)
printf '%s\n' "$y" | wc -c

var=word # comment
case "$var" in #inline comment
  word) #inline comment3
    echo word #inline comment2
    ;; #cmmt
  *) #cmmt
    echo not-word #cmmt
    ;; #cmmt
esac  #cmmt

case && in
esac

# testv=test123
# true
# cat <<EOF
#   this is a test
#   this should expand to test123: $testv
#   this is a number $?
# EOF

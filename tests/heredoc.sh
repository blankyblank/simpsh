#!/home/blank/dev/simpsh/simpsh

testv=test123
true
cat <<EOF
  this is a test
  this should expand to test123: $testv
  this is a number $?
EOF

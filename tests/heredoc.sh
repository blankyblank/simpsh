#!/home/blank/dev/simpsh/simpsh

testv=test123
cat <<EOF
  this is a test
  this should expand to test123: $testv
EOF

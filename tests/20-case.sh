#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

msg_run ' word=thisisatest; case "$word" in t*) echo "pass" ;; *) echo "fail" ;; esac '
out1=$(../simpsh -c ' word=thisisatest; case "$word" in t*) echo "pass" ;; *) echo "fail" ;; esac ')
if [ "$out1" = "pass" ]; then
  test_pass "out1" "matches" "pass"
else
  test_fail "out1" "expected" "pass"
  exit 1
fi

msg_run 'case a|b multi-pattern: case b in (a|b)'
out=$(../simpsh -c 'case b in (a|b) echo m;; (*) echo n;; esac')
if [ "$out" = "m" ]; then test_pass "out" "matches" "m"; else
  test_fail "out" "expected" "m"
  exit 1
fi

msg_run 'case [!...] negation: case h in ([!aeiou])'
out=$(../simpsh -c 'case h in ([!aeiou]) echo m;; (*) echo n;; esac')
if [ "$out" = "m" ]; then test_pass "out" "matches" "m"; else
  test_fail "out" "expected" "m"
  exit 1
fi

msg_run 'case [[:alpha:]] class'
out=$(../simpsh -c 'case h in ([[:alpha:]]) echo m;; (*) echo n;; esac')
if [ "$out" = "m" ]; then test_pass "out" "matches" "m"; else
  test_fail "out" "expected" "m"
  exit 1
fi

msg_run 'case [[:space:]] rejects non-space'
out=$(../simpsh -c 'case h in ([[:space:]]) echo m;; (*) echo n;; esac')
if [ "$out" = "n" ]; then test_pass "out" "matches" "n"; else
  test_fail "out" "expected" "n"
  exit 1
fi

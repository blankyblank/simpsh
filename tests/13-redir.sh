#!/bin/sh
# shellcheck disable=2016

[ -f ./funcs ] && . ./funcs

f="./redir.txt"
errf="./redir.err"
trap 'rm -f "$f" "$errf"' EXIT

# === Basic redirections ===

msg_run "echo test123 > $f ; cat $f"
out1=$(printf '%s\n' "echo test123 > $f" "cat $f" | ../simpsh)
if [ "$out1" != "test123" ]; then
  test_fail "out1" "differs from" "test123"
  exit 1
else
  test_pass "out1" "matches" "test123"
fi

msg_run ">> append twice"
../simpsh -c "echo hello >> $f"
out2=$(cat "$f")
if [ "$out2" != "test123
hello" ]; then
  msg_fail "\$out2=$out2 differs from 'test123\nhello'"
  exit 1
else
  msg_pass ">> append preserves prior content"
fi
rm -f "$f"

msg_run ">> to non-existent file creates it"
rm -f "$f"
../simpsh -c "echo first >> $f"
content=$(cat "$f")
if [ "$content" != "first" ]; then
  test_fail "content" "differs from" "first"
  exit 1
else
  test_pass "content" "matches" "first"
fi
rm -f "$f"

msg_run "cat < $f (input redirection)"
echo "input_test" > "$f"
out=$(../simpsh -c "cat < $f")
if [ "$out" != "input_test" ]; then
  test_fail "out" "differs from" "input_test"
  exit 1
else
  test_pass "out" "matches" "input_test"
fi
rm -f "$f"

msg_run "echo err >&2 ; 2> $errf"
rm -f "$errf"
../simpsh -c "echo err >&2" > /dev/null 2> "$errf"
content=$(cat "$errf")
if [ "$content" != "err" ]; then
  test_fail "content" "differs from" "err"
  exit 1
else
  test_pass "content" "matches" "err"
fi
rm -f "$errf"

msg_run "set -C; echo new >| $f (clobber override)"
echo "original" > "$f"
../simpsh -c "set -C; echo new >| $f"
content=$(cat "$f")
if [ "$content" != "new" ]; then
  test_fail "content" "differs from" "new"
  exit 1
else
  test_pass "content" "matches" "new"
fi
rm -f "$f"

msg_run "echo out >&2 (dup stdout to stderr)"
out=$(../simpsh -c "echo out >&2" 2>&1 >/dev/null)
if [ "$out" != "out" ]; then
  test_fail "out" "differs from" "out"
  exit 1
else
  test_pass "out" "matches" "out"
fi

msg_run "echo err >&2 (dup stderr to stdout)"
out=$(../simpsh -c "echo err >&2" 2>/dev/null)
if [ "$out" != "" ]; then
  msg_fail "2>/dev/null did not suppress stderr: got '$out'"
  exit 1
else
  err=$(../simpsh -c "echo err >&2" 2>&1 >/dev/null)
  if [ "$err" != "err" ]; then
    test_fail "err" "differs from" "err"
    exit 1
  else
    test_pass "err" "matches" "err"
  fi
fi

# === Close-FD (>&- / <&- ) ===

msg_run "echo >&- (close stdout)"
rm -f "$errf"
../simpsh -c 'echo >&-' 2> "$errf"
rc=$?
if [ $rc -ne 0 ] && grep -q "Bad file descriptor\|could not write" "$errf"; then
  msg_pass ">&- closes stdout"
else
  msg_fail ">&- (rc=$rc, err=$(cat "$errf"))"
  exit 1
fi
rm -f "$errf"

msg_run "cat <&- (close stdin)"
rm -f "$errf"
../simpsh -c 'cat <&-' 2> "$errf"
rc=$?
if [ $rc -ne 0 ] && grep -q "Bad file descriptor" "$errf"; then
  msg_pass "<&- closes stdin"
else
  msg_fail "<&- (rc=$rc, err=$(cat "$errf"))"
  exit 1
fi
rm -f "$errf"

# === Redirect ordering ===

msg_run ">out 2>err (separated)"
rm -f "$f" "$errf"
../simpsh -c "echo to_stdout; echo to_stderr >&2" > "$f" 2> "$errf"
if [ "$(cat "$f")" = "to_stdout" ] && [ "$(cat "$errf")" = "to_stderr" ]; then
  msg_pass "separate >out 2>err"
else
  msg_fail "out=$(cat "$f") err=$(cat "$errf")"
  exit 1
fi
rm -f "$f" "$errf"

msg_run ">out 2>&1 (combine, both to file)"
rm -f "$f"
../simpsh -c "echo to_stdout; echo to_stderr >&2" > "$f" 2>&1
if [ "$(cat "$f")" = "to_stdout
to_stderr" ]; then
  msg_pass "combine >out 2>&1"
else
  msg_fail "$(cat "$f")"
  exit 1
fi
rm -f "$f"

msg_run "2>&1 >out (POSIX ordering: stderr stays at terminal)"
rm -f "$f" "$errf"
../simpsh -c "echo to_stdout; echo to_stderr >&2" 2> "$errf" > "$f"
# 2>&1 dups stderr to current stdout (terminal) BEFORE >out redirects stdout
# So stderr should land in errf (captured terminal) and stdout in f
if [ "$(cat "$f")" = "to_stdout" ] && [ "$(cat "$errf")" = "to_stderr" ]; then
  msg_pass "2>&1 >out ordering"
else
  msg_fail "out=$(cat "$f") err=$(cat "$errf")"
  exit 1
fi
rm -f "$f" "$errf"

# === Cflag / noclobber ===

msg_run "set -C; echo new > $f (blocked)"
echo "original" > "$f"
../simpsh -c "set -C; echo new > $f" 2>/dev/null || true
content=$(cat "$f")
if [ "$content" != "original" ]; then
  test_fail "content" "differs from" "original"
  exit 1
else
  test_pass "content" "matches" "original"
fi
rm -f "$f"

# === Heredocs ===

msg_run "cat << EOF (basic)"
out=$(../simpsh -c 'cat << EOF
hello world
EOF')
if [ "$out" != "hello world" ]; then
  test_fail "out" "differs from" "hello world"
  exit 1
else
  test_pass "out" "matches" "hello world"
fi

msg_run "cat << EOF (multi-line preserves newlines)"
out=$(../simpsh -c 'cat << EOF
a
b
c
EOF')
if [ "$out" = "a
b
c" ]; then
  msg_pass "heredoc multi-line"
else
  msg_fail "heredoc multi-line: $out"
  exit 1
fi

msg_run "cat << EOF (no trailing newline in input)"
out=$(../simpsh -c 'cat << EOF
single
EOF')
if [ "$out" != "single" ]; then
  test_fail "out" "differs from" "single"
  exit 1
else
  test_pass "out" "matches" "single"
fi

msg_run "cat <<- EOF (tab-stripped)"
out=$(../simpsh -c 'cat <<- EOF
	indented
		moreindented
EOF')
if [ "$out" = "indented
		moreindented" ]; then
  msg_pass "heredoc <<- strips leading tabs"
else
  msg_fail "heredoc <<-: $out"
  exit 1
fi

msg_run "cat <<'EOF' (quoted, no expansion)"
out=$(env tmpvar=expanded ../simpsh -c "cat << 'EOF'
\$tmpvar
EOF")
if [ "$out" != '$tmpvar' ]; then
  test_fail "out" "differs from" '$tmpvar'
  exit 1
else
  test_pass "out" "matches" '$tmpvar'
fi

msg_run "cat <<EOF (unquoted, expansion)"
out=$(env tmpvar=expanded ../simpsh -c 'cat << EOF
$tmpvar
EOF')
if [ "$out" != "expanded" ]; then
  test_fail "out" "differs from" "expanded"
  exit 1
else
  test_pass "out" "matches" "expanded"
fi

exit 0

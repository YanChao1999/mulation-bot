#!/bin/sh
# CLI / campaign smoke checks against a built runner + instrumented binary.
# Usage: run-cli-tests.sh <mulation-run> <test-binary>
set -eu
runner=${1:?runner}
binary=${2:?binary}

fail() {
    echo "[  FAILED  ] $*" >&2
    exit 1
}

ok() {
    echo "[       OK ] $*"
}

"$runner" --help >/dev/null 2>&1 || fail "help"
ok Cli.Help

set +e
"$runner" --min-score >/dev/null 2>&1
code=$?
set -e
[ "$code" -eq 2 ] || fail "missing --min-score value exit=$code"
ok Cli.MinScoreRequiresValue

set +e
"$runner" --not-a-flag "$binary" >/dev/null 2>&1
code=$?
set -e
[ "$code" -eq 2 ] || fail "unknown flag exit=$code"
ok Cli.UnknownFlag

json=$(mktemp)
trap 'rm -f "$json"' EXIT
"$runner" --min-score 0 --json-out "$json" "$binary" >/dev/null
grep -q '"killed"' "$json" || fail "json missing killed"
grep -q '"survived"' "$json" || fail "json missing survived"
grep -q '"score"' "$json" || fail "json missing score"
ok Cli.JsonOut

set +e
"$runner" --min-score 100 "$binary" >/dev/null 2>&1
code=$?
set -e
[ "$code" -eq 1 ] || fail "low score should fail --min-score 100 exit=$code"
ok Cli.MinScoreGate

set +e
"$runner" --git-diff=this-revision-does-not-exist-mulation "$binary" >/dev/null 2>&1
code=$?
set -e
[ "$code" -eq 2 ] || fail "bad git-diff should fail closed exit=$code"
ok Cli.GitDiffFailClosed

set +e
out=$("$runner" --min-score 0 --filter this-path-does-not-match-any-mutant "$binary" 2>&1)
code=$?
set -e
[ "$code" -eq 0 ] || fail "empty --filter should exit 0 exit=$code"
echo "$out" | grep -q 'nothing to prove' || fail "empty --filter should say nothing to prove"
ok Cli.FilterEmpty

"$runner" --min-score 0 --kinds ROR -j 2 "$binary" >/dev/null || fail "--kinds ROR campaign"
ok Cli.KindsRor

id=$("$runner" --min-score 0 --json-out "$json" --kinds ROR "$binary" >/dev/null && \
  sed -n 's/.*"id":\([0-9][0-9]*\).*/\1/p' "$json" | head -n1)
[ -n "$id" ] || fail "could not extract mutant id from json"
"$runner" --min-score 0 --mutant-id "$id" "$binary" >/dev/null || fail "--mutant-id campaign"
ok Cli.MutantId

"$runner" --help 2>&1 | grep -q -- '--test-filter' || fail "help missing --test-filter"
"$runner" --help 2>&1 | grep -q -- '--discover-cmd' || fail "help missing --discover-cmd"
"$runner" --help 2>&1 | grep -q -- '--allow-empty-coverage' || fail "help missing --allow-empty-coverage"
ok Cli.NewFlagsInHelp

echo "[  PASSED  ] cli smoke"

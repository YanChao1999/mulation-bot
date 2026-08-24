#!/bin/sh
# Drive `make test`: runner units, runtime, then a mutation campaign.
# Usage: run-tests.sh [test-binary]
# The campaign is the general CLI: mulation-run --min-score 0 <binary>
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$root"
BUILD=${BUILD:-build}
binary=${1:-}

ok() {
    name=$1
    shift
    echo "[ RUN      ] $name"
    "$@"
    echo "[       OK ] $name"
}

fail() {
    echo "[  FAILED  ] $*" >&2
    exit 1
}

echo "=============================================="
echo " 1) Runner unit tests"
echo "    catalog / git-diff / process"
echo "=============================================="
"$BUILD/mulation_unit_tests"

echo
echo "=============================================="
echo " 2) Runtime (MULATION_MUTANT / MULATION_HITLOG)"
echo "=============================================="
ok Runtime.Inactive env -u MULATION_MUTANT "$BUILD/runtime_test" 1 0
ok Runtime.ActiveMatch env MULATION_MUTANT=42 "$BUILD/runtime_test" 42 1
ok Runtime.ActiveMiss env MULATION_MUTANT=42 "$BUILD/runtime_test" 7 0
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT
ok Runtime.HitLog env MULATION_HITLOG="$tmp" env -u MULATION_MUTANT \
    "$BUILD/runtime_test" 1 0 10 20
grep -q '^10$' "$tmp" && grep -q '^20$' "$tmp" || fail "Runtime.HitLog missing ids"
echo "[  PASSED  ] 4 runtime test(s)"

echo
echo "=============================================="
echo " 3) Mutation campaign"
echo "    mulation-run --min-score 0 <test-binary>"
echo "=============================================="
if [ -z "$binary" ]; then
    fail "usage: $0 <test-binary>"
fi
"$BUILD/mulation-run" --min-score 0 "$binary"

echo
echo "=============================================="
echo " make test: all passed"
echo "=============================================="

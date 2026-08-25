#!/bin/sh
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$root"

fmt=$(command -v clang-format-18 2>/dev/null || command -v clang-format 2>/dev/null || true)
if [ -z "$fmt" ]; then
    if [ "${CI:-}" = "true" ] || [ "${REQUIRE_CLANG_TOOLS:-}" = "1" ]; then
        echo "error: clang-format-18 not found (apt install clang-format-18)" >&2
        exit 1
    fi
    echo "skip format: clang-format not installed"
    exit 0
fi

# shellcheck disable=SC2046
files=$(sh scripts/list-sources.sh)
if [ "${1:-}" = "--check" ]; then
    echo "== clang-format --dry-run ($fmt) =="
    $fmt --dry-run -Werror $files
else
    echo "== clang-format -i ($fmt) =="
    $fmt -i $files
fi

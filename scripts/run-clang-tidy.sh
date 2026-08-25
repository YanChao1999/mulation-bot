#!/bin/sh
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$root"

tidy=$(command -v clang-tidy-18 2>/dev/null || command -v clang-tidy 2>/dev/null || true)
if [ -z "$tidy" ]; then
    if [ "${CI:-}" = "true" ] || [ "${REQUIRE_CLANG_TOOLS:-}" = "1" ]; then
        echo "error: clang-tidy-18 not found (apt install clang-tidy-18)" >&2
        exit 1
    fi
    echo "skip clang-tidy: not installed"
    exit 0
fi

echo "== clang-tidy ($tidy) =="
status=0
for f in runner/*.cpp tests/*.cpp; do
    [ -f "$f" ] || continue
    $tidy -quiet "$f" -- \
        -std=c++17 -Irunner -Itests -Iinclude \
        -Wall -Wextra -Wpedantic || status=1
done
for f in runtime/*.c tests/*.c; do
    [ -f "$f" ] || continue
    $tidy -quiet "$f" -- \
        -std=c11 -Iinclude \
        -Wall -Wextra -Wpedantic || status=1
done
exit "$status"

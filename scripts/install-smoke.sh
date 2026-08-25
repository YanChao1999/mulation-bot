#!/bin/sh
# Verify `make install` produces a usable PATH-style layout:
# bin/mulation finds ../lib plugin + runtime via /proc/self/exe.
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$root"

PREFIX=${PREFIX:-}
if [ -z "$PREFIX" ]; then
    PREFIX=$(mktemp -d /tmp/mulation-install-XXXXXX)
    cleanup=1
else
    cleanup=0
fi

echo "== install-smoke: PREFIX=$PREFIX =="
make install PREFIX="$PREFIX"

bin="$PREFIX/bin/mulation"
plugin="$PREFIX/lib/libmulation_plugin.so"
runtime="$PREFIX/lib/libmulation_runtime.a"
hdr="$PREFIX/include/mulation/mulation.h"
cmake_mod="$PREFIX/lib/cmake/Mulation/Mulation.cmake"

for p in "$bin" "$plugin" "$runtime" "$hdr" "$cmake_mod"; do
    if [ ! -e "$p" ]; then
        echo "missing: $p" >&2
        exit 1
    fi
done

"$bin" --help >/dev/null 2>&1

workdir=$(mktemp -d /tmp/mulation-smoke-src-XXXXXX)
trap 'rm -rf "$workdir"; [ "$cleanup" = 1 ] && rm -rf "$PREFIX"' EXIT

cat >"$workdir/add.cpp" <<'EOF'
int add2(int a, int b) { return a + b; }
EOF
cat >"$workdir/main.cpp" <<'EOF'
int add2(int, int);
int main() { return add2(2, 2) == 4 ? 0 : 1; }
EOF

"$bin" c++ -std=c++17 -O0 -g -c "$workdir/add.cpp" -o "$workdir/add.o"
"$bin" c++ -std=c++17 -O0 -g "$workdir/add.o" "$workdir/main.cpp" -o "$workdir/t"
"$workdir/t"

"$PREFIX/bin/mulation-run" --min-score 0 "$workdir/t" >/dev/null

echo "[  PASSED  ] install-smoke"

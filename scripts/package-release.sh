#!/bin/sh
# Build a relocatable install tree and tarball for GitHub Releases.
# Requires a prior `make all` (or builds it). Host must provide LLVM 18 at runtime.
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
cd "$root"

VERSION=${VERSION:-}
if [ -z "$VERSION" ]; then
    if [ -n "${GITHUB_REF_NAME:-}" ] && [ "${GITHUB_REF_TYPE:-}" = "tag" ]; then
        VERSION=$GITHUB_REF_NAME
    else
        VERSION=$(git describe --tags --always --dirty 2>/dev/null || echo 0.0.0-dev)
    fi
fi
VERSION=${VERSION#v}

ARCH=${ARCH:-$(uname -m)}
case "$ARCH" in
x86_64 | amd64) ARCH=x86_64 ;;
aarch64 | arm64) ARCH=aarch64 ;;
esac
OS=${OS:-linux}
PKG_NAME="mulation-bot-${VERSION}-${OS}-${ARCH}"

DIST=${DIST:-dist}
STAGE="$DIST/$PKG_NAME"
rm -rf "$STAGE"
mkdir -p "$DIST"

echo "== package: $PKG_NAME =="
make all
make install PREFIX="$root/$STAGE"

cp -f README.md LICENSE "$STAGE/"
cat >"$STAGE/INSTALL.txt" <<EOF
mulation-bot ${VERSION} (${OS}-${ARCH})

Layout:
  bin/mulation, bin/mulation-run
  lib/libmulation_plugin.so, lib/libmulation_runtime.a
  include/mulation/mulation.h
  lib/cmake/Mulation/Mulation.cmake

Requirements:
  - Linux
  - Clang/LLVM 18 development libraries at runtime for the pass plugin
    (e.g. Ubuntu 24.04: sudo apt install clang-18 llvm-18)

Install:
  tar xf ${PKG_NAME}.tar.gz
  sudo cp -a ${PKG_NAME}/bin/* /usr/local/bin/
  sudo cp -a ${PKG_NAME}/lib/* /usr/local/lib/
  sudo cp -a ${PKG_NAME}/include/mulation /usr/local/include/
  # or: PREFIX=\$HOME/.local and copy into \$PREFIX/{bin,lib,include}

Use:
  CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
  mulation --git-diff --min-score 80 -- ./build/my_tests

Docs: https://yanchao1999.github.io/mulation-bot/
EOF

TARBALL="$DIST/${PKG_NAME}.tar.gz"
tar -C "$DIST" -czf "$TARBALL" "$PKG_NAME"
(
    cd "$DIST"
    sha256sum "$(basename "$TARBALL")" >"${PKG_NAME}.sha256"
)

echo "wrote $TARBALL"
echo "wrote $DIST/${PKG_NAME}.sha256"
ls -la "$TARBALL" "$DIST/${PKG_NAME}.sha256"

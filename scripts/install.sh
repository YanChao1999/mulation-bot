#!/bin/sh
# Install mulation-bot from a GitHub Release tarball into PREFIX (default: $HOME/.local).
#
#   curl -fsSL https://raw.githubusercontent.com/YanChao1999/mulation-bot/v0.0.1/scripts/install.sh | sh
#
# Environment:
#   MULATION_VERSION  Release tag (default: latest), e.g. v0.0.1
#   PREFIX            Install root (default: $HOME/.local)
#   MULATION_REPO     GitHub repo (default: YanChao1999/mulation-bot)
set -eu

REPO=${MULATION_REPO:-YanChao1999/mulation-bot}
PREFIX=${PREFIX:-${HOME}/.local}
VERSION=${MULATION_VERSION:-}

fail() {
    echo "mulation install: $*" >&2
    exit 1
}

need_cmd() {
    command -v "$1" >/dev/null 2>&1 || fail "missing required command: $1"
}

case "$(uname -s)" in
Linux) ;;
*) fail "unsupported OS: $(uname -s) (Linux x86_64 only today)" ;;
esac

ARCH=$(uname -m)
case "$ARCH" in
x86_64 | amd64) ARCH=x86_64 ;;
*) fail "unsupported CPU: $ARCH (linux-x86_64 releases only today)" ;;
esac

need_cmd curl
need_cmd tar
need_cmd mkdir
need_cmd cp

if [ -z "$VERSION" ]; then
    VERSION=$(
        curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" |
            sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' |
            head -n 1
    )
    [ -n "$VERSION" ] || fail "could not resolve latest release for ${REPO}"
fi

ver="${VERSION#v}"
pkg="mulation-bot-${ver}-linux-${ARCH}"
base="https://github.com/${REPO}/releases/download/${VERSION}"
tarball="${base}/${pkg}.tar.gz"
sha_url="${base}/${pkg}.sha256"

work=$(mktemp -d /tmp/mulation-install-XXXXXX)
trap 'rm -rf "$work"' EXIT

echo "== mulation install: ${VERSION} -> ${PREFIX} =="
curl -fsSL "$tarball" -o "$work/${pkg}.tar.gz"

if curl -fsSL "$sha_url" -o "$work/${pkg}.sha256" 2>/dev/null; then
    (
        cd "$work"
        sha256sum -c "${pkg}.sha256"
    )
else
    echo "note: no ${pkg}.sha256 on release; skipping checksum"
fi

tar -C "$work" -xf "$work/${pkg}.tar.gz"
stage="$work/$pkg"
[ -d "$stage/bin" ] || fail "unexpected tarball layout (missing bin/)"

mkdir -p "$PREFIX/bin" "$PREFIX/lib" "$PREFIX/include"
cp -a "$stage/bin/." "$PREFIX/bin/"
cp -a "$stage/lib/." "$PREFIX/lib/"
if [ -d "$stage/include/mulation" ]; then
    mkdir -p "$PREFIX/include"
    cp -a "$stage/include/mulation" "$PREFIX/include/"
fi

if ! command -v clang++-18 >/dev/null 2>&1 && ! command -v clang-18 >/dev/null 2>&1; then
    echo "warning: clang-18 not found on PATH — the pass plugin needs LLVM 18 at runtime"
    echo "  Ubuntu 24.04: sudo apt install clang-18 llvm-18"
fi

case ":${PATH}:" in
*":${PREFIX}/bin:"*) ;;
*)
    echo ""
    echo "Add to PATH:"
    echo "  export PATH=\"${PREFIX}/bin:\$PATH\""
    ;;
esac

if [ -x "${PREFIX}/bin/mulation" ]; then
    "${PREFIX}/bin/mulation" --help >/dev/null 2>&1 || true
fi

echo "== installed ${VERSION} =="
echo "  bin:     ${PREFIX}/bin/mulation"
echo "  lib:     ${PREFIX}/lib/libmulation_plugin.so"
echo "  include: ${PREFIX}/include/mulation/mulation.h"

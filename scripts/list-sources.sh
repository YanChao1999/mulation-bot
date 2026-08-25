#!/bin/sh
# List C/C++ sources for clang-format / clang-tidy (repo-relative).
cd "$(dirname "$0")/.." || exit 1
find runner runtime include tests examples plugin \
  \( -name '*.c' -o -name '*.h' -o -name '*.cpp' -o -name '*.hpp' \) \
  ! -path '*/build/*' | sort

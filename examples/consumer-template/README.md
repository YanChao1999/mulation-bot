# Consumer CI template

Copy-paste mutation gate for a C++ repo that already uses gtest / CTest / doctest (or any host ELF suite).

## 1. One-time install on the runner (or locally)

```bash
curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.6/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
sudo apt install clang-18 llvm-18-dev   # pass plugin needs LLVM 18
```

## 2. Build the SUT with instrumentation

```bash
# optional: skip home-grown UT headers when using CXX="mulation c++"
# export MULATION_SKIP_PATHS=third_party/my_ut.h
CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
ctest --test-dir build          # must stay green
```

CMake helpers (optional):

```cmake
include(Mulation)
mulation_instrument(my_lib)
mulation_add_check(my_tests MIN_SCORE 80 TEST_FILTER Adult.* JOBS 4)
```

## 3. Drop in the workflow

Copy [`mulation-gate.yml`](mulation-gate.yml) to your repo as:

```text
.github/workflows/mulation-gate.yml
```

Edit `TEST_BINARY` (and optional `MIN_SCORE`) at the top of that file.

## What you get

- PR / push runs: install `v0.0.6` → instrumented rebuild of *your* steps → `mulation --git-diff -j 4`
- Survived mutants on the PR diff become **review comments** (`comment-survivors`)
- Score below `MIN_SCORE` fails the check

Useful local flags once the binary exists:

```bash
mulation --git-diff --min-score 80 --filter src/ --kinds ROR,AOR -- ./build/my_tests
mulation --test-filter Adult.* -- ./build/my_tests
mulation --discover-cmd 'find build -name "libmy_sut.so"' -- ./build/my_tests
```

See also: [weak → strong guide](../../docs/weak-to-strong.md).

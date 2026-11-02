# Consumer CI template

Copy-paste mutation gate for a C++ repo that already uses Google Test / CTest.

## 1. One-time install on the runner (or locally)

```bash
curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.3/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
sudo apt install clang-18 llvm-18   # pass plugin needs LLVM 18
```

## 2. Build the SUT with instrumentation

```bash
CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
ctest --test-dir build          # must stay green
```

## 3. Drop in the workflow

Copy [`mulation-gate.yml`](mulation-gate.yml) to your repo as:

```text
.github/workflows/mulation-gate.yml
```

Edit `TEST_BINARY` (and optional `MIN_SCORE`) at the top of that file.

## What you get

- PR / push runs: install `v0.0.3` → instrumented rebuild of *your* steps → `mulation --git-diff -j 4`
- Survived mutants on the PR diff become **review comments** (`comment-survivors`)
- Score below `MIN_SCORE` fails the check

See also: [weak → strong guide](../../docs/weak-to-strong.md).

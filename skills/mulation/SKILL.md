---
name: mulation
description: >-
  Wire and run mulation-bot mutation gates on host ELF unit suites (gtest, CTest,
  doctest, Zephyr ZTEST, custom). Use when adding a mutation CI gate, interpreting
  survivors, instrumenting a SUT with Clang/LLVM 18, or debugging empty hitlogs /
  shared-library mutant discovery.
---

# mulation mutation gate

mulation is a **layer** on existing unit tests — not a test framework. Wrap a green
host ELF (or `ctest`) the same way you wrap Valgrind.

## When to use

- Add or harden a PR mutation gate (`--git-diff --min-score`)
- Instrument a C/C++ SUT with the Clang pass + runtime
- Consume survivors (weak → strong assertions)
- Zephyr ZTEST on `native_sim`, doctest, or a custom harness

## Quick gate

```bash
# after: suite green, SUT built with instrumentation
mulation --git-diff --min-score 80 -j 4 -- ./build/my_tests
# or
mulation --git-diff --min-score 80 -j 4 -- ctest --test-dir build
```

Keep campaigns cheap on PRs: prefer `--git-diff`, then slim with `--filter` /
`--exclude` / `--kinds` / `--test-filter` / `--mutant-id` / `--max-survivors`.
Emit `--sarif-out` for GitHub Code Scanning. Skip noisy sites with
`// mulation:off` … `// mulation:on` or `// mulation:skip` on a line.

## Instrument the SUT only

```bash
export MULATION_SKIP_PATHS=third_party/my_ut.h   # optional custom UT headers
CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
```

Or CMake:

```cmake
include(Mulation)
mulation_instrument(my_lib)                        # whole target
mulation_instrument_sources(app SOURCES src/foo.c) # Zephyr-style
mulation_add_check(my_tests MIN_SCORE 80 TEST_FILTER Adult.* JOBS 4)
# creates mulation-check-my_tests + umbrella mulation-check
```

Test sources (`*_test.*`, `test_*.*`) and `gtest` / `doctest.h` / Zephyr `ztest`
paths are skipped by the pass. Do **not** rely on skipping a project directory
named `doctest/` — only the `doctest.h` header is skipped.

## Framework filters

```bash
mulation --test-filter Adult.* -- ./build/my_gtest          # → --gtest_filter=
mulation --framework=doctest --test-filter '*Adult*' -- ./t # → -tc=
mulation --test-filter compare::adult -- ./zephyr.exe       # → -test=
mulation --framework=custom --test-filter only -- ./harness # exports MULATION_TEST_FILTER
```

## Discovery

- ELF `cmd[0]` and `ctest --show-only=json-v1` binaries are scanned.
- Linked `.so` with a `mulation_mutants` section are found via `DT_NEEDED` + `RUNPATH`/`RPATH`/`$ORIGIN`.
- Non-ELF orchestrators: `--binary PATH` and/or `--discover-cmd 'find … -print'`.

## Coverage hitlog empty

Default: **fail closed** if the coverage run is green but `MULATION_HITLOG` is empty
(often `_exit` / skipped `atexit`). Fix the exit path, or use `--allow-empty-coverage`
/ `--no-coverage` deliberately.

## Build this repo

```bash
sudo apt install clang-18 llvm-18-dev make
export LLVM_CONFIG=/usr/lib/llvm-18/bin/llvm-config
make -j$(nproc) CXX=clang++-18 CC=clang-18 PLUGIN_CXX=clang++-18
make test CXX=clang++-18 CC=clang-18 PLUGIN_CXX=clang++-18
```

`llvm-config --libs` must resolve (Ubuntu: `-lLLVM-18`).

## Interpreting survivors

See `docs/weak-to-strong.md`: kill meaningful faults with the smallest assertion;
do not pad tests only to chase 100%. Timeouts are reported but excluded from score.

## Examples in-tree

- `examples/gtest-ctest/`
- `examples/doctest/`
- `examples/zephyr-ztest-native/` (prefer `native_sim/native/64`; ELF32 catalogs work)
- `examples/consumer-template/` (GitHub Actions gate)

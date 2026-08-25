# mulation-bot

A mutation-testing **layer** on Google Test and CTest — not a unit-test framework replacement.

| | Google Test | CTest | mulation-bot |
|---|---|---|---|
| Role | Write tests | Run binaries | Score how useful those tests are |
| Question | Does current code pass? | Did every test exit 0? | If this operator were wrong, would a test fail? |
| Green suite | Necessary | Necessary | Not enough by itself |
| Gate | Tests passed | Tests passed | Tests passed **and** mutants killed |

Coverage says a line **ran**. Mutation says: if that line were **faulty**, a test would **fail**.

## Recommended CI gate

Keep campaigns cheap: only mutants on the PR/worktree diff, fail if the score is too low.

```bash
mulation --git-diff --min-score 80 -- ./build/my_tests
# or
mulation --git-diff --min-score 80 -- ctest --test-dir build
```

Wire that after a green `ctest` / gtest run. Full-suite mutation without `--git-diff` is usually too slow for every PR.

## Valgrind-style usage

Same habit as Valgrind — wrap the test command:

```bash
valgrind --leak-check=full ./my_gtest
mulation --min-score 80 ./my_gtest
mulation -- ctest
```

Product code must be built with instrumentation (Clang/LLVM pass). Test sources are skipped.

```bash
make && make install          # or: PREFIX=$HOME/.local make install
export PATH="$HOME/.local/bin:$PATH"   # if not installing to /usr/local

CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
ctest --test-dir build
mulation --git-diff --min-score 80 -- ./build/my_tests
```

CMake helper (optional; CTest stays the unit runner):

```cmake
list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/Mulation")  # after make install
include(Mulation)
mulation_instrument(my_lib)       # SUT only
mulation_add_check(my_tests)      # cmake --build . --target mulation-check
```

## Build & verify

**Supported today:** Linux + Clang/LLVM 18. Not GCC/MSVC instrumentation yet.

```bash
sudo apt install clang-18 llvm-18-dev   # plugin build
# optional: clang-format-18 clang-tidy-18

make                 # plugin, runtime, mulation / mulation-run
make test            # quiet: runner + runtime + one mulation-run on the example binary
make install-smoke   # install to a temp prefix and check the wrapper finds assets
make check           # test + format-check + tidy
```

Verbose internal unit names: `MULATION_TEST_VERBOSE=1 make test`.

```bash
make install PREFIX=/usr/local          # needs write access to PREFIX
make install PREFIX=$HOME/.local
```

Install layout:

- `bin/mulation`, `bin/mulation-run`
- `lib/libmulation_plugin.so`, `lib/libmulation_runtime.a`
- `include/mulation/mulation.h`
- `lib/cmake/Mulation/Mulation.cmake`

The runner resolves plugin/runtime via `/proc/self/exe` (so `PATH`-installed `mulation` still finds `../lib/...`).

## What the report means

```
Mulation report
  mutants:   N
  killed:    K
  survived:  S
  score:     K/(K+S) %
```

- **killed** — test failed, crashed, or timed out under that mutant
- **survived** — suite still green (or site never hit; uncovered counts as survived)
- **`--min-score N`** — exit non-zero if score is below N
- **`--git-diff [BASE]`** — only mutants on lines changed vs BASE (default `HEAD`); fails closed on a bad revision

The tool does not invent per-operator advice. Survived lines are locations for you to strengthen assertions if the mutant is meaningful.

## Mutation operators (MVP)

- **AOR** — `+`/`-`, `*`/`/`, `%`
- **ROR** — `<`/`<=`/`>`/`>=`/`==`/`!=`
- **BOR** — bitwise `&`/`|` (non-i1 LLVM `and`/`or`)
- **LCR** — i1 `and`/`or` (source `&&`/`||` is often control flow and may not appear as a binop)

## Limitations (why you might not use this yet)

- **Clang plugin required** — not a true Valgrind-style DBI; SUT must rebuild with `-fpass-plugin`.
- **Linux + LLVM 18** — other toolchains are out of scope for now.
- **Signal noise** — equivalent mutants (behavior-preserving edits) inflate “survived”; treat the score as a guide, not absolute truth.
- **Cost** — without `--git-diff` / coverage filtering, large suites get expensive.
- **Young tool** — fewer operators and less history than mull / mutate++; niche is a thin gtest/CTest gate, not full mutation research.

## Example

`examples/gtest-ctest/` is a demo green suite with a weak boundary (surviving `>=` → `>`). It is not special-cased in the runner; any instrumented test binary works the same way.

```bash
make test
# == mulation ==
# ... score from mulation-run on build/compare_tests
```

## Layout

- `plugin/` — LLVM pass (`-fpass-plugin=libmulation_plugin.so`)
- `runtime/` — `mulation_active(id)`, env `MULATION_MUTANT` / `MULATION_HITLOG`
- `runner/` — `mulation` / `mulation-run`
- `tests/` — unit tests for catalog, git-diff, process, runtime
- `cmake/` — `Mulation.cmake`, ClangTools
- `examples/gtest-ctest/` — sample SUT + gtest-style tests
- `scripts/` — format, tidy, install-smoke

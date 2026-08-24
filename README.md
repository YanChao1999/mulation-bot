# mulation-bot

A mutation-testing **layer** on Google Test and CTest. It does not replace either tool. It answers a different question:

| | Google Test | CTest (CMake) | mulation-bot |
|---|---|---|---|
| Role | Write tests (`TEST`, `EXPECT_*`) | Run test binaries and report pass/fail | Evaluate the **tests**, not only the code |
| Question | Does current code match my assertions? | Did every registered test exit 0? | If I inject a realistic bug, does a test fail? |
| Green suite | Necessary | Necessary | Not sufficient — a green suite can still miss bugs |
| Extra pipeline | Compile → run | Orchestrate binaries | Compile **once** with a compiler plugin → flip mutants at runtime → mutation score |
| Production gate | Tests passed | Tests passed | Tests passed **and** mutants on changed code are killed |

Coverage says a line **ran**. Mutation says: if that line were **faulty**, a test would **fail**. That is how mulation proves a test is useful and reduces the chance of shipping a weak suite.

## Valgrind-style binary

`mulation` / `mulation-run` is a standalone wrapper, same habit as Valgrind:

```bash
valgrind --leak-check=full ./my_gtest
mulation --min-score 80 ./my_gtest
mulation -- ctest
```

You keep writing Google Test (or any CTest binary that exits 0 on pass). Mulation instruments **product code only**, never test files.

Unlike Valgrind, mutants need compile-time instrumentation (a Clang/LLVM pass plugin). Build the SUT through the wrapper, then run:

```bash
CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
ctest --test-dir build          # unit tests — must stay green
mulation --min-score 80 --git-diff -- ./build/compare_tests
```

Or with the CMake helper (CTest unchanged; `mulation-check` is the pre-production gate):

```cmake
include(Mulation)
mulation_instrument(my_lib)      # SUT only
mulation_add_check(my_tests)     # cmake --build . --target mulation-check
```

## Build

Requires Clang/LLVM 18 (pass plugin) and a C++17 compiler. For format and tidy:

```bash
sudo apt install clang-format-18 clang-tidy-18
```

```bash
make                 # plugin, runtime, mulation-run
make test            # unit tests for the runner and runtime
make format          # clang-format -i
make format-check    # clang-format --dry-run -Werror
make tidy            # clang-tidy on runner, runtime, and tests
make lint            # -Wall -Wextra -Wpedantic -Werror, then clang-tidy
make check           # tests + example mutation campaign + format-check + lint
```

CMake (optional):

```bash
cmake -B build -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
cmake --build build
ctest --test-dir build
cmake --build build --target format-check
cmake --build build --target tidy
cmake --build build --target mulation-check
```

Compile with tidy on each translation unit: `cmake -B build -DMULATION_ENABLE_CLANG_TIDY=ON`.

## What the report means

- **killed** — a test failed or crashed: the suite noticed the bug
- **survived** — tests still passed: this class of bug could reach production
- **timeout** — counted as killed (the suite “noticed” via hang)
- **score** — `killed / (killed + survived)`

`--min-score N` fails CI when the score is too low. `--git-diff [BASE]` only scores mutants on the PR/worktree diff.

A survived mutant at `foo.cpp:42` `[ROR] >= -> >` means: add a boundary assertion that distinguishes `>=` from `>`.

## Mutation operators (MVP)

- **AOR** arithmetic: `+`/`-`, `*`/`/`, `%`
- **ROR** relational: `<`/`<=`/`>`/`>=`/`==`/`!=`
- **LCR** logical: `&&`/`||`
- **LVR** literals: `0`↔`1` on compares and returns

Equivalent mutants (behavior-preserving edits) can inflate “survived”; treat them as test debt for now.

## Layout

- `plugin/` — LLVM pass plugin (`-fpass-plugin=libmulation_plugin.so`)
- `runtime/` — C ABI `mulation_active(id)`, env `MULATION_MUTANT`
- `runner/` — `mulation` / `mulation-run` CLI
- `tests/` — unit tests for catalog, git-diff, process, and runtime
- `cmake/Mulation.cmake` — `mulation_instrument` / `mulation_add_check`
- `cmake/ClangTools.cmake` — `format`, `format-check`, `tidy` CMake targets
- `.clang-format` / `.clang-tidy` — LLVM-style format and tidy checks
- `scripts/run-clang-format.sh` / `scripts/run-clang-tidy.sh`
- `examples/gtest-ctest/` — green suite with a surviving boundary mutant

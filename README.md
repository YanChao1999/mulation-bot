# mulation-bot

A mutation-testing **layer** on Google Test and CTest — not a unit-test framework replacement.

**Site:** https://yanchao1999.github.io/mulation-bot/ · **Releases:** https://github.com/YanChao1999/mulation-bot/releases

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

## Install from a GitHub Release

One command (Linux x86_64, installs to `$HOME/.local`):

```bash
curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.1/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

Pin a version or prefix:

```bash
MULATION_VERSION=v0.0.1 PREFIX=$HOME/.local sh -c 'curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.1/install.sh | sh'
```

Manual install from the release tarball:

```bash
# download mulation-bot-*-linux-x86_64.tar.gz from Releases, then:
tar xf mulation-bot-*-linux-x86_64.tar.gz
sudo cp -a mulation-bot-*/bin/* /usr/local/bin/
sudo cp -a mulation-bot-*/lib/* /usr/local/lib/
sudo cp -a mulation-bot-*/include/mulation /usr/local/include/
sudo apt install clang-18 llvm-18   # pass plugin needs LLVM 18
```

Or build from source:

```bash
make && make install PREFIX=$HOME/.local
export PATH="$HOME/.local/bin:$PATH"
```

## Valgrind-style usage

Same habit as Valgrind — wrap the test command:

```bash
valgrind --leak-check=full ./my_gtest
mulation --min-score 80 ./my_gtest
mulation -- ctest
```

Product code must be built with instrumentation (Clang/LLVM pass). Test sources are skipped.

```bash
CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
ctest --test-dir build
mulation --git-diff --min-score 80 -- ./build/my_tests
```

CMake helper (optional; CTest stays the unit runner):

```cmake
list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/Mulation")  # after install
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
make test            # quiet: unit + runtime + mulation on example + self (runner lib)
make install-smoke   # install to a temp prefix and check the wrapper finds assets
make package         # dist/mulation-bot-*-linux-*.tar.gz (+ .sha256)
make check           # test + format-check + tidy
```

Verbose internal unit names: `MULATION_TEST_VERBOSE=1 make test`.

`make test` dogfoods the tool: an instrumented `mulation_self_tests` binary (runner library + existing unit suite) is scored with `mulation-run --min-score 0`. Do not pad unit assertions just to raise that score — survivors are the signal.

Publish a release: push a tag `v0.0.1` (workflow builds, tests, uploads the tarball + `install.sh`).  
GitHub Pages: enable **Settings → Pages → Source: GitHub Actions** (workflow deploys `docs/`).

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

The same idea applies to **mulation-bot itself**: `make test` scores an instrumented runner-library binary (`build/mulation_self_tests`: catalog/diff/cli + matching unit tests). Report the score; do not invent stronger cases only to kill mutants. Process stays on the plain unit suite only (timeouts would explode self-campaign time).

```bash
make test
# == mulation (example) ==
# ... score on build/compare_tests
# == mulation (self) ==
# ... score on build/mulation_self_tests
```

## Layout

- `plugin/` — LLVM pass (`-fpass-plugin=libmulation_plugin.so`)
- `runtime/` — `mulation_active(id)`, env `MULATION_MUTANT` / `MULATION_HITLOG`
- `runner/` — `mulation` / `mulation-run`
- `tests/` — unit suite for catalog, git-diff, process, cli, runtime; also drives self-mutation
- `cmake/` — `Mulation.cmake`, ClangTools
- `examples/gtest-ctest/` — sample SUT + gtest-style tests
- `docs/` — GitHub Pages site
- `scripts/` — format, tidy, install, install-smoke, package-release
- `.github/workflows/` — `ci.yml`, `release.yml`, `pages.yml`

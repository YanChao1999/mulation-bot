# mulation-bot

A mutation-testing **layer** on Google Test, CTest, doctest, Zephyr ZTEST, and other host ELF runners — not a unit-test framework replacement.

**Site:** https://yanchao1999.github.io/mulation-bot/ · **Releases:** https://github.com/YanChao1999/mulation-bot/releases

Agent materials: Cursor skill [`.cursor/skills/mulation/`](.cursor/skills/mulation/) (also under [`skills/mulation/`](skills/mulation/)). Other agent packs are pinned with [abom](https://github.com/YanChao1999/abom) — see [`docs/abom.md`](docs/abom.md) and `abom.lock.json`.

| | Google Test / doctest / ZTEST | CTest | mulation-bot |
|---|---|---|---|
| Role | Write tests | Run binaries | Score how useful those tests are |
| Question | Does current code pass? | Did every test exit 0? | If this operator were wrong, would a test fail? |
| Green suite | Necessary | Necessary | Not enough by itself |
| Gate | Tests passed | Tests passed | Tests passed **and** mutants killed |

Coverage says a line **ran**. Mutation says: if that line were **faulty**, a test would **fail**.

## Recommended CI gate

Keep campaigns cheap: only mutants on the PR/worktree diff, fail if the score is too low.

```bash
mulation --git-diff --min-score 80 -j 4 -- ./build/my_tests
# or
mulation --git-diff --min-score 80 -j 4 -- ctest --test-dir build
# slim the mutant set further when useful:
mulation --git-diff --filter src/ --kinds ROR,AOR -- ./build/my_tests
# fail-fast triage + Code Scanning:
mulation --git-diff --min-score 80 --max-survivors 5 \
  --json-out report.json --sarif-out report.sarif -- ./build/my_tests
```

`-j` / `--jobs N` runs mutant replays in parallel (default `1`).
`--max-survivors N` stops scheduling after N survivors (partial score; good for PR triage).
`--sarif-out` writes SARIF 2.1.0 for GitHub Code Scanning.

Skip instrumentation on known-equivalent or noisy lines:

```c
// mulation:off
int noisy = a + b;  // not mutated
// mulation:on
int edge = x >= 18; // mulation:skip
```

Wire that after a green unit run. Full-suite mutation without `--git-diff` is usually too slow for every PR.

Unified test subset (or pass framework-native args yourself):

```bash
mulation --test-filter Adult.* -- ./build/my_gtest
mulation --framework=doctest --test-filter '*Adult*' -- ./build/my_doctest
mulation --test-filter compare::adult_boundary -- ./build/zephyr/zephyr.exe
# custom harness: exports MULATION_TEST_FILTER
mulation --framework=custom --test-filter only_me -- ./my_harness
```

## Install from a GitHub Release

One command (Linux x86_64, installs to `$HOME/.local`):

```bash
# Prefer pinning so GitHub API rate limits cannot break resolve-latest:
MULATION_VERSION=v0.0.6 sh -c 'curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.6/install.sh | sh'
export PATH="$HOME/.local/bin:$PATH"
```

Pin a prefix (and always pass `MULATION_VERSION` into the `sh -c` so the installer sees it):

```bash
MULATION_VERSION=v0.0.6 PREFIX=$HOME/.local sh -c 'curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.6/install.sh | sh'
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

Product code must be built with instrumentation (Clang/LLVM pass). Test sources and
common framework headers (`gtest`, `doctest`, Zephyr/`ztest`) are skipped. For a
home-grown UT header or third-party tree, set colon-separated path substrings:

```bash
export MULATION_SKIP_PATHS=third_party/my_ut.h:vendor/fakeit
CXX="mulation c++" CC="mulation cc" cmake -B build && cmake --build build
ctest --test-dir build
mulation --git-diff --min-score 80 -- ./build/my_tests
```

CMake helper (optional; CTest stays the unit runner):

```cmake
list(APPEND CMAKE_MODULE_PATH "/usr/local/lib/cmake/Mulation")  # after install
include(Mulation)
mulation_instrument(my_lib)                          # SUT only
mulation_instrument_sources(app SOURCES src/foo.c)   # Zephyr-style app targets
mulation_add_check(my_tests ARGS --gtest_filter=Adult.*)
# also: TEST_FILTER / FRAMEWORK / JOBS / BINARIES / COMMAND
# builds mulation-check-my_tests + umbrella target mulation-check
```

When the SUT is a shared library, mutants are picked up from `DT_NEEDED` libs that
embed a `mulation_mutants` section (or pass `--binary` / `--discover-cmd`).

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

Publish a release: push a tag `v0.0.6` (workflow builds, tests, uploads the tarball + `install.sh`).  
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
  timeout:   T   (excluded from score)
  score:     K/(K+S) %
```

- **killed** — test failed or crashed under that mutant
- **survived** — suite still green (or site never hit; uncovered counts as survived)
- **timeout** — per-mutant deadline hit (often loop-index mutants); reported but **not** scored as killed, so hangs do not inflate the gate
- **`--min-score N`** — exit non-zero if score is below N
- **`--git-diff [BASE]`** — only mutants on lines changed vs BASE (default `HEAD`); fails closed on a bad revision
- **`--filter SUBSTR`** / **`--exclude SUBSTR`** — keep/drop mutants by source path substring (repeatable)
- **`--kinds LIST`** — comma-separated operators to keep (`ROR,AOR,...`)
- **`--mutant-id ID`** — replay a single mutant (repeatable)
- **`--test-filter PAT`** / **`--framework NAME`** — subset tests via gtest/ctest/doctest/ztest/custom
- **`--discover-cmd CMD`** — `sh -c CMD`; each stdout line is an extra ELF to scan
- **`--allow-empty-coverage`** — if hitlog is empty, run all mutants (default: fail closed)
- **`-j` / `--jobs N`** — parallel mutant workers

The tool does not invent per-operator advice. Survived lines are locations for you to strengthen assertions if the mutant is meaningful.

## Mutation operators (MVP)

- **AOR** — `+`/`-`/`*`/`/`/`%` with dual + one cross-family alternative (e.g. `+`→`-` and `+`→`*`)
- **ROR** — `<`/`<=`/`>`/`>=`/`==`/`!=` with boundary flip, opposite relational, and equality
- **BOR** — bitwise `&`/`|` (non-i1 LLVM `and`/`or`)
- **LCR** — i1 `and`/`or` (source `&&`/`||` is often control flow and may not appear as a binop)
- **LVR** — integer literals `0`↔`1`

Prefer `--git-diff` so the denser operator set stays cheap on PRs.

## GitHub Actions gate (copy-paste)

Fastest path for a new repo: copy the single-job template

[`examples/consumer-template/mulation-gate.yml`](examples/consumer-template/mulation-gate.yml)
→ `.github/workflows/mulation-gate.yml`, then edit `TEST_BINARY` / `MIN_SCORE`.

Guide: [`examples/consumer-template/README.md`](examples/consumer-template/README.md).

Reusable workflow (when the instrumented binary is already built in a prior job):

```yaml
jobs:
  mulation:
    uses: YanChao1999/mulation-bot/.github/workflows/mulation.yml@v0.0.6
    with:
      test-binary: ./build/my_tests   # already built with mulation c++
      min-score: "80"
      git-diff: true
      comment-survivors: true         # PR review comments on survived lines
    permissions:
      contents: read
      pull-requests: write
```

See [docs/weak-to-strong.md](docs/weak-to-strong.md) for turning survivors into stronger assertions.

## Limitations (why you might not use this yet)

- **Clang plugin required** — not a true Valgrind-style DBI; SUT must rebuild with `-fpass-plugin`.
- **Linux + LLVM 18** — other toolchains are out of scope for now.
- **Signal noise** — equivalent mutants (behavior-preserving edits) inflate “survived”; treat the score as a guide, not absolute truth.
- **Cost** — without `--git-diff` / coverage filtering, large suites get expensive.
- **Empty hitlog fails closed** — if coverage cannot flush `MULATION_HITLOG` (`_exit` / skipped `atexit`), the run errors unless `--allow-empty-coverage` or `--no-coverage`.
- **Young tool** — fewer operators and less history than mull / mutate++; niche is a thin host-ELF gate, not full mutation research.

## Example

`examples/gtest-ctest/` and `examples/doctest/` are demo green suites with a weak boundary (surviving `>=` → `>`). They are not special-cased in the runner; any instrumented host ELF that exits non-zero on failure works the same way.

**Zephyr / ZTEST:** use a host `native_sim` binary the same way — see
[`examples/zephyr-ztest-native/`](examples/zephyr-ztest-native/) (prefer `native_sim/native/64`; 32-bit host ELFs are also readable).

The same idea applies to **mulation-bot itself**: `make test` scores an instrumented runner-library binary (`build/mulation_self_tests`: catalog/diff + matching unit tests). Report the score; do not invent stronger cases only to kill mutants. Process and CLI stay uninstrumented in the self binary (loop-index mutants hang and flood timeouts into the score).

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
- `examples/doctest/` — sample SUT + doctest-style tests
- `examples/zephyr-ztest-native/` — Zephyr ZTEST on `native_sim` + mulation wiring
- `examples/consumer-template/` — copy-paste GitHub Actions gate + PR comments
- `docs/` — GitHub Pages site (`index.html`) + weak-to-strong guide
- `skills/mulation/` · `.cursor/skills/mulation/` — agent skill for wiring gates
- `scripts/` — format, tidy, install, install-smoke, package-release, pr-comment-survivors
- `.github/workflows/` — `ci.yml`, `release.yml`, `pages.yml`, `mulation-gate.yml`, reusable `mulation.yml`

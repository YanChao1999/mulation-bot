# Zephyr + ZTEST + mulation (`native_sim`)

Small advantage over gtest-only mutation tools: the same Valgrind-style gate works on
**Zephyr ZTEST** when you run on a **host** board (`native_sim` / historically `native_posix`).

mulation does not replace ZTEST or Twister. It wraps the host ELF Twister/`west` already produces.

```text
west build -b native_sim
./build/zephyr/zephyr.exe          # ZTEST must exit 0
mulation --git-diff -j 4 -- ./build/zephyr/zephyr.exe
```

QEMU / hardware targets are out of scope for this MVP (cross-plugin + per-mutant target runs).

## Prerequisites

- Zephyr SDK / workspace (`ZEPHYR_BASE`)
- **LLVM/Clang 18** as the Zephyr toolchain (`ZEPHYR_TOOLCHAIN_VARIANT=llvm` or equivalent)
- mulation installed ([install.sh](https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.3/install.sh))

```bash
curl -fsSL https://github.com/YanChao1999/mulation-bot/releases/download/v0.0.3/install.sh | sh
export PATH="$HOME/.local/bin:$PATH"
```

## Layout (copy into your app)

| File | Role |
|------|------|
| [`CMakeLists.txt`](CMakeLists.txt) | Zephyr app; pass plugin only on SUT sources |
| [`prj.conf`](prj.conf) | Enable ZTEST |
| [`src/compare.c`](src/compare.c) / [`compare.h`](src/compare.h) | Product code (instrumented) |
| [`src/compare_test.c`](src/compare_test.c) | ZTEST suite (not instrumented; keep `*_test.c` naming) |
| [`mulation-gate.yml`](mulation-gate.yml) | CI snippet — set `TEST_BINARY` to `zephyr.exe` |

## Build & gate

```bash
export ZEPHYR_TOOLCHAIN_VARIANT=llvm   # must be Clang for -fpass-plugin
west build -p always -b native_sim \
  -d build-native \
  examples/zephyr-ztest-native

# Unit tests (ZTEST) — must stay green
./build-native/zephyr/zephyr.exe

# Mutation gate (same binary)
mulation --git-diff --min-score 80 -j 4 -- \
  ./build-native/zephyr/zephyr.exe
```

Adjust `-d` / paths to match your west workspace. Some trees name the binary
`zephyr.elf` instead of `zephyr.exe` — point `mulation` at whichever host ELF you get.

## Intentionally weak ZTEST

[`src/compare_test.c`](src/compare_test.c) only checks `is_adult(20)`. Expect survivors such as
`>=` → `>` until you add boundary asserts (see [weak → strong](../../docs/weak-to-strong.md)):

```c
ZTEST(compare, adult_boundary) {
  zassert_true(is_adult(18));
  zassert_false(is_adult(17));
}
```

## Twister

Host cases that exec the same instrumented `native_sim` binary can be gated the same way:

```bash
mulation --git-diff -j 4 --min-score 80 -- \
  ./build-native/zephyr/zephyr.exe
```

Do **not** point mulation at a QEMU-wrapped Twister invocation unless every mutant still runs as a host process with a clear exit code.

## Pitfalls

1. **GCC Zephyr toolchain** — no Clang pass; switch to LLVM.
2. **Instrumenting all of Zephyr** — too slow / noisy; only SUT `.c` via `set_source_files_properties` (see CMakeLists).
3. **Always-zero exit** — if the runner swallows ZTEST failures, fix that before mutation.
4. **C++ apps** — same pattern; use `mulation c++` / `.cpp` sources with the pass plugin.

# Weak tests → strong tests

mulation does not invent assertions for you. A **survived** mutant means: the suite stayed green after a deliberate fault. That is a hole in the contract your tests encode.

## The classic hole

```c
int is_adult(int age) { return age >= 18; }
```

```cpp
TEST(Adult, TwentyIsAdult) { EXPECT_TRUE(is_adult(20)); }  // green + often 100% coverage
```

Mutant `>=` → `>` still passes. Coverage lied; mutation did not.

## Strengthen the boundary

```cpp
TEST(Adult, Boundary) {
  EXPECT_TRUE(is_adult(18));
  EXPECT_FALSE(is_adult(17));
}
```

That pair kills the ROR boundary mutants. Prefer **on/off pairs** at the contract edge over a single happy-path value.

## How to read a survivor

| Report | What to add |
|--------|-------------|
| `>=` → `>` or `>` → `>=` | Inclusive vs exclusive boundary (N and N−1) |
| `==` → `!=` | A case that must match exactly, and one that must not |
| `+` → `-` / `*` → `/` | A numeric oracle the suite already knows (golden value) |
| uncovered | A test that executes that line, then a contract assertion |

## Workflow

1. Keep unit tests green (`ctest` / gtest / doctest / Zephyr ZTEST on `native_sim`).
2. Run a cheap campaign on the PR diff (optionally slim further):

   ```bash
   mulation --git-diff --min-score 80 --filter src/ --kinds ROR,AOR \
     --test-filter Adult.* -- ./build/my_tests
   ```

   Or `./build/doctest_compare_tests` / `./build/zephyr/zephyr.exe`.
3. For each survivor on the PR diff: add the smallest assertion that would fail under that operator.
4. Re-run. Do **not** pad tests only to chase 100% — kill meaningful faults; leave equivalents alone.

## Coverage hitlog empty?

If mulation exits with “coverage hitlog is empty”, the process likely skipped `atexit` (e.g. `_exit` / some `native_sim` paths), so `MULATION_HITLOG` never flushed. Prefer a normal `exit`, or temporarily use `--allow-empty-coverage` / `--no-coverage` while you fix the exit path.

## Skip noisy / equivalent sites

When a site is intentionally equivalent or too noisy for a gate:

```c
// mulation:off
int hash = a + b;   // not instrumented until on
// mulation:on
int edge = x >= 18; // mulation:skip
```

## PR comments / SARIF

With the reusable workflow (`comment-survivors: true`), survived sites become review comments on the changed lines. Set `upload-sarif: true` to push `--sarif-out` into GitHub Code Scanning. Treat survivors like review nits: either strengthen the test or document why the mutant is equivalent (or mute it).

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

1. Keep unit tests green (`ctest` / gtest / Zephyr ZTEST on `native_sim`).
2. `mulation --git-diff --min-score 80 -- ./build/my_tests`
   (or `./build/zephyr/zephyr.exe` for Zephyr host tests)
3. For each survivor on the PR diff: add the smallest assertion that would fail under that operator.
4. Re-run. Do **not** pad tests only to chase 100% — kill meaningful faults; leave equivalents alone.

## PR comments

With the reusable workflow (`comment-survivors: true`), survived sites become review comments on the changed lines. Treat them like review nits: either strengthen the test or document why the mutant is equivalent.

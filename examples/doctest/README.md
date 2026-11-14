# doctest + mulation

Same Valgrind-style gate as the gtest example: instrument the SUT, keep a green
doctest suite, then wrap the test binary with `mulation`.

```bash
make example
./build/doctest_compare_tests
mulation --min-score 0 -j 2 -- ./build/doctest_compare_tests
```

Pass framework-native filters through as trailing args:

```bash
mulation -- ./build/doctest_compare_tests -tc=*Adult*
```

Tests live in `tests/compare_test.cpp` (skipped by the pass via `*_test.cpp`).
The bundled `doctest.h` is a minimal harness; files named `doctest.h` are
skipped when using `CXX="mulation c++"` (project dirs like `examples/doctest/`
are still instrumented).

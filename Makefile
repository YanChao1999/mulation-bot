LLVM_CONFIG ?= /usr/lib/llvm-18/bin/llvm-config
CXX ?= clang++
CC ?= clang
BUILD ?= build
PREFIX ?= /usr/local
WARNFLAGS ?= -Wall -Wextra -Wpedantic

PLUGIN := $(BUILD)/libmulation_plugin.so
RUNTIME := $(BUILD)/libmulation_runtime.a
RUNNER := $(BUILD)/mulation-run
MULATION := $(BUILD)/mulation
EXAMPLE := $(BUILD)/compare_tests
UNIT := $(BUILD)/mulation_unit_tests
RUNTIME_TEST := $(BUILD)/runtime_test
PLUGIN_CXX := clang++
LLVM_LIBDIR := $(shell $(LLVM_CONFIG) --libdir)

PLUGIN_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags) -fPIC -fno-rtti
RUNNER_SRC := runner/main.cpp runner/catalog.cpp runner/process.cpp runner/diff.cpp
UNIT_SRC := tests/unit_main.cpp tests/catalog_test.cpp tests/diff_test.cpp tests/process_test.cpp \
	runner/catalog.cpp runner/diff.cpp runner/process.cpp

.PHONY: all example test lint tidy format format-check check clean install install-smoke package

all: $(PLUGIN) $(RUNTIME) $(RUNNER) $(MULATION)

$(BUILD):
	mkdir -p $(BUILD)

$(PLUGIN): plugin/MulationPass.cpp | $(BUILD)
	$(PLUGIN_CXX) -shared -fPIC $(PLUGIN_CXXFLAGS) $< -o $@ \
		-L$(LLVM_LIBDIR) -lLLVM \
		-Wl,-rpath,$(LLVM_LIBDIR)

$(BUILD)/mulation_runtime.o: runtime/mulation_runtime.c include/mulation/mulation.h | $(BUILD)
	$(CC) -std=c11 -c -O2 -fPIC $(WARNFLAGS) -Werror -Iinclude $< -o $@

$(RUNTIME): $(BUILD)/mulation_runtime.o
	ar rcs $@ $<

$(RUNNER): $(RUNNER_SRC) runner/catalog.hpp runner/process.hpp runner/diff.hpp | $(BUILD)
	$(CXX) -std=c++17 -O2 $(WARNFLAGS) -Werror -Irunner $(RUNNER_SRC) -o $@

$(MULATION): $(RUNNER)
	ln -sfn mulation-run $@

$(UNIT): $(UNIT_SRC) tests/unit_check.hpp runner/catalog.hpp runner/diff.hpp runner/process.hpp | $(BUILD)
	$(CXX) -std=c++17 -O2 $(WARNFLAGS) -Werror -Irunner -Itests $(UNIT_SRC) -o $@

$(RUNTIME_TEST): tests/runtime_test.c $(RUNTIME) include/mulation/mulation.h | $(BUILD)
	$(CC) -std=c11 -O2 $(WARNFLAGS) -Werror -Iinclude tests/runtime_test.c $(RUNTIME) -o $@

$(BUILD)/compare.o: examples/gtest-ctest/src/compare.cpp examples/gtest-ctest/src/compare.h $(PLUGIN) | $(BUILD)
	clang++ -std=c++17 -O0 -g -fPIC $(WARNFLAGS) -Werror -fpass-plugin=$(abspath $(PLUGIN)) \
		-Iexamples/gtest-ctest/src -c $< -o $@

$(BUILD)/compare_test.o: examples/gtest-ctest/tests/compare_test.cpp \
		examples/gtest-ctest/mini_gtest.h examples/gtest-ctest/src/compare.h | $(BUILD)
	clang++ -std=c++17 -O0 -g -fPIC $(WARNFLAGS) -Werror -Iexamples/gtest-ctest -Iexamples/gtest-ctest/src -c $< -o $@

$(EXAMPLE): $(BUILD)/compare.o $(BUILD)/compare_test.o $(RUNTIME)
	clang++ $^ -o $@

example: $(EXAMPLE) $(RUNNER)

test: $(UNIT) $(RUNTIME_TEST) example
	@echo "== runner =="
	$(UNIT)
	@echo "== runtime =="
	@env -u MULATION_MUTANT $(RUNTIME_TEST) 1 0
	@MULATION_MUTANT=42 $(RUNTIME_TEST) 42 1
	@MULATION_MUTANT=42 $(RUNTIME_TEST) 7 0
	@tmp=$$(mktemp); \
	  MULATION_HITLOG=$$tmp env -u MULATION_MUTANT $(RUNTIME_TEST) 1 0 10 20 && \
	  grep -q '^10$$' $$tmp && grep -q '^20$$' $$tmp && rm -f $$tmp
	@echo "[  PASSED  ] runtime"
	@echo "== mulation =="
	$(RUNNER) --min-score 0 $(EXAMPLE)

lint: $(RUNNER) $(UNIT) $(RUNTIME_TEST) tidy
	@echo "== compiler lint (-Wall -Wextra -Wpedantic -Werror) ok =="

tidy:
	sh scripts/run-clang-tidy.sh

format:
	sh scripts/run-clang-format.sh

format-check:
	sh scripts/run-clang-format.sh --check

check: test format-check lint
	@echo "== make check: tests + format + tidy ok =="

install-smoke: all
	sh scripts/install-smoke.sh

package: all
	sh scripts/package-release.sh

clean:
	rm -rf $(BUILD) dist

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include/mulation
	install -m 0755 $(RUNNER) $(DESTDIR)$(PREFIX)/bin/mulation-run
	ln -sfn mulation-run $(DESTDIR)$(PREFIX)/bin/mulation
	install -m 0755 $(PLUGIN) $(DESTDIR)$(PREFIX)/lib/libmulation_plugin.so
	install -m 0644 $(RUNTIME) $(DESTDIR)$(PREFIX)/lib/libmulation_runtime.a
	install -m 0644 include/mulation/mulation.h $(DESTDIR)$(PREFIX)/include/mulation/mulation.h
	install -d $(DESTDIR)$(PREFIX)/lib/cmake/Mulation
	install -m 0644 cmake/Mulation.cmake $(DESTDIR)$(PREFIX)/lib/cmake/Mulation/Mulation.cmake

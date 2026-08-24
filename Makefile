LLVM_CONFIG ?= /usr/lib/llvm-18/bin/llvm-config
CXX ?= clang++
CC ?= clang
BUILD ?= build
PREFIX ?= /usr/local

PLUGIN := $(BUILD)/libmulation_plugin.so
RUNTIME := $(BUILD)/libmulation_runtime.a
RUNNER := $(BUILD)/mulation-run
MULATION := $(BUILD)/mulation
EXAMPLE := $(BUILD)/compare_tests
PLUGIN_CXX := clang++
LLVM_LIBDIR := $(shell $(LLVM_CONFIG) --libdir)

PLUGIN_CXXFLAGS := $(shell $(LLVM_CONFIG) --cxxflags) -fPIC -fno-rtti
RUNNER_SRC := runner/main.cpp runner/catalog.cpp runner/process.cpp runner/diff.cpp

.PHONY: all example check clean install

all: $(PLUGIN) $(RUNTIME) $(RUNNER) $(MULATION)

$(BUILD):
	mkdir -p $(BUILD)

$(PLUGIN): plugin/MulationPass.cpp | $(BUILD)
	$(PLUGIN_CXX) -shared -fPIC $(PLUGIN_CXXFLAGS) $< -o $@ \
		-L$(LLVM_LIBDIR) -lLLVM \
		-Wl,-rpath,$(LLVM_LIBDIR)

$(BUILD)/mulation_runtime.o: runtime/mulation_runtime.c include/mulation/mulation.h | $(BUILD)
	$(CC) -c -O2 -fPIC -Iinclude $< -o $@

$(RUNTIME): $(BUILD)/mulation_runtime.o
	ar rcs $@ $<

$(RUNNER): $(RUNNER_SRC) runner/catalog.hpp runner/process.hpp runner/diff.hpp | $(BUILD)
	$(CXX) -std=c++17 -O2 -Irunner $(RUNNER_SRC) -o $@

$(MULATION): $(RUNNER)
	ln -sfn mulation-run $@

$(BUILD)/compare.o: examples/gtest-ctest/src/compare.cpp examples/gtest-ctest/src/compare.h $(PLUGIN) | $(BUILD)
	clang++ -std=c++17 -O0 -g -fPIC -fpass-plugin=$(abspath $(PLUGIN)) \
		-Iexamples/gtest-ctest/src -c $< -o $@

$(BUILD)/compare_test.o: examples/gtest-ctest/tests/compare_test.cpp \
		examples/gtest-ctest/mini_gtest.h examples/gtest-ctest/src/compare.h | $(BUILD)
	clang++ -std=c++17 -O0 -g -fPIC -Iexamples/gtest-ctest -Iexamples/gtest-ctest/src -c $< -o $@

$(EXAMPLE): $(BUILD)/compare.o $(BUILD)/compare_test.o $(RUNTIME)
	clang++ $^ -o $@

example: $(EXAMPLE)

check: example $(RUNNER)
	@echo "== unit tests (CTest-style binary, must pass) =="
	$(EXAMPLE)
	@echo "== mutation campaign (prove tests are useful) =="
	$(RUNNER) --min-score 0 $(EXAMPLE)

clean:
	rm -rf $(BUILD)

install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib $(DESTDIR)$(PREFIX)/include/mulation
	install -m 0755 $(RUNNER) $(DESTDIR)$(PREFIX)/bin/mulation-run
	ln -sfn mulation-run $(DESTDIR)$(PREFIX)/bin/mulation
	install -m 0755 $(PLUGIN) $(DESTDIR)$(PREFIX)/lib/libmulation_plugin.so
	install -m 0644 $(RUNTIME) $(DESTDIR)$(PREFIX)/lib/libmulation_runtime.a
	install -m 0644 include/mulation/mulation.h $(DESTDIR)$(PREFIX)/include/mulation/mulation.h
	install -d $(DESTDIR)$(PREFIX)/lib/cmake/Mulation
	install -m 0644 cmake/Mulation.cmake $(DESTDIR)$(PREFIX)/lib/cmake/Mulation/Mulation.cmake

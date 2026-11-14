#ifndef MULATION_TEST_FILTER_HPP
#define MULATION_TEST_FILTER_HPP

#include "cli.hpp"

#include <string>
#include <vector>

// Resolve --framework=auto against cmd[0], then append native filter args for
// --test-filter. Returns false and sets *err on unknown --framework values.
bool apply_test_filter(Options &opt, std::string *err);

// Exposed for unit tests.
std::string detect_framework(const std::vector<std::string> &cmd, const std::string &forced);
std::vector<std::string> native_test_filter_args(const std::string &framework,
                                                 const std::string &pattern);

#endif

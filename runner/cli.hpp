#ifndef MULATION_CLI_HPP
#define MULATION_CLI_HPP

#include <cstdint>
#include <string>
#include <vector>

struct Options {
    int min_score = 0;
    bool min_score_set = false;
    bool git_diff = false;
    std::string git_base = "HEAD";
    int timeout_ms = 0;
    int jobs = 1;
    bool no_coverage = false;
    bool allow_empty_coverage = false;
    std::string catalog_dir;
    std::string json_out;
    std::string discover_cmd;
    std::string test_filter;
    std::string framework = "auto"; // auto|gtest|ctest|doctest|ztest|custom
    std::string resolved_framework; // filled by apply_test_filter
    std::vector<std::string> binaries;
    std::vector<std::string> filter;
    std::vector<std::string> exclude;
    std::vector<std::string> kinds;
    std::vector<uint32_t> mutant_ids;
    std::vector<std::string> cmd;
};

enum class ParseKind { Ok, Help, Version, Error };

struct ParseResult {
    ParseKind kind = ParseKind::Ok;
    Options options;
    std::string message;
};

std::string usage_text(const char *argv0);
ParseResult parse_args(int argc, char **argv);

#endif

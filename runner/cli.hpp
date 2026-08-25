#ifndef MULATION_CLI_HPP
#define MULATION_CLI_HPP

#include <string>
#include <vector>

struct Options {
    int min_score = 0;
    bool min_score_set = false;
    bool git_diff = false;
    std::string git_base = "HEAD";
    int timeout_ms = 0;
    bool no_coverage = false;
    std::string catalog_dir;
    std::string json_out;
    std::vector<std::string> binaries;
    std::vector<std::string> cmd;
};

enum class ParseKind { Ok, Help, Error };

struct ParseResult {
    ParseKind kind = ParseKind::Ok;
    Options options;
    std::string message;
};

std::string usage_text(const char *argv0);
ParseResult parse_args(int argc, char **argv);

#endif

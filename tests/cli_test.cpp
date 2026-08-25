#include "cli.hpp"
#include "unit_check.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace {

ParseResult parse(std::vector<const char *> args) {
    args.insert(args.begin(), "mulation");
    return parse_args(static_cast<int>(args.size()), const_cast<char **>(args.data()));
}

} // namespace

TEST(Cli, Help) {
    ParseResult r = parse({"--help"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Help));
    CHECK(r.message.find("Usage:") != std::string::npos);
}

TEST(Cli, MinScoreEqualsAndFlag) {
    ParseResult a = parse({"--min-score=80", "./tests"});
    CHECK_EQ(static_cast<int>(a.kind), static_cast<int>(ParseKind::Ok));
    CHECK(a.options.min_score_set);
    CHECK_EQ(a.options.min_score, 80);
    CHECK_EQ(a.options.cmd.size(), 1u);

    ParseResult b = parse({"--min-score", "55", "--", "./bin"});
    CHECK_EQ(static_cast<int>(b.kind), static_cast<int>(ParseKind::Ok));
    CHECK_EQ(b.options.min_score, 55);
    CHECK_EQ(b.options.cmd[0], std::string("./bin"));
}

TEST(Cli, MinScoreMissingValueIsError) {
    ParseResult r = parse({"--min-score"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Error));
    CHECK(r.message.find("requires a value") != std::string::npos);
}

TEST(Cli, GitDiffDefaultAndEquals) {
    ParseResult a = parse({"--git-diff", "./t"});
    CHECK(a.options.git_diff);
    CHECK_EQ(a.options.git_base, std::string("HEAD"));

    ParseResult b = parse({"--git-diff=origin/main", "./t"});
    CHECK(b.options.git_diff);
    CHECK_EQ(b.options.git_base, std::string("origin/main"));
}

TEST(Cli, NoCoverageBinaryAndJsonOut) {
    ParseResult r =
        parse({"--no-coverage", "--binary", "/tmp/a", "--json-out", "/tmp/out.json", "./suite"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Ok));
    CHECK(r.options.no_coverage);
    CHECK_EQ(r.options.binaries.size(), 1u);
    CHECK_EQ(r.options.binaries[0], std::string("/tmp/a"));
    CHECK_EQ(r.options.json_out, std::string("/tmp/out.json"));
    CHECK_EQ(r.options.cmd[0], std::string("./suite"));
}

TEST(Cli, UnknownOptionIsError) {
    ParseResult r = parse({"--not-a-real-flag", "./t"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Error));
    CHECK(r.message.find("unknown option") != std::string::npos);
}

TEST(Cli, DoubleDashCollectsCommand) {
    ParseResult r = parse({"--min-score=10", "--", "ctest", "--test-dir", "build"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Ok));
    CHECK_EQ(r.options.cmd.size(), 3u);
    CHECK_EQ(r.options.cmd[0], std::string("ctest"));
    CHECK_EQ(r.options.cmd[2], std::string("build"));
}

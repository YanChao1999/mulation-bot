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

TEST(Cli, JobsFlag) {
    ParseResult a = parse({"-j", "4", "./t"});
    CHECK_EQ(static_cast<int>(a.kind), static_cast<int>(ParseKind::Ok));
    CHECK_EQ(a.options.jobs, 4);

    ParseResult b = parse({"--jobs=8", "./t"});
    CHECK_EQ(b.options.jobs, 8);

    ParseResult c = parse({"--jobs", "0", "./t"});
    CHECK_EQ(static_cast<int>(c.kind), static_cast<int>(ParseKind::Error));
}

TEST(Cli, MutantFilters) {
    ParseResult r = parse({"--filter", "src/", "--filter=lib/", "--exclude", "third_party",
                           "--kinds", "ROR,AOR", "--mutant-id", "7", "--mutant-id=9", "./t"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Ok));
    CHECK_EQ(r.options.filter.size(), 2u);
    CHECK_EQ(r.options.filter[0], std::string("src/"));
    CHECK_EQ(r.options.filter[1], std::string("lib/"));
    CHECK_EQ(r.options.exclude.size(), 1u);
    CHECK_EQ(r.options.exclude[0], std::string("third_party"));
    CHECK_EQ(r.options.kinds.size(), 2u);
    CHECK_EQ(r.options.kinds[0], std::string("ROR"));
    CHECK_EQ(r.options.kinds[1], std::string("AOR"));
    CHECK_EQ(r.options.mutant_ids.size(), 2u);
    CHECK_EQ(r.options.mutant_ids[0], 7u);
    CHECK_EQ(r.options.mutant_ids[1], 9u);
}

TEST(Cli, FilterRequiresValue) {
    ParseResult r = parse({"--filter"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Error));
}

TEST(Cli, TestFilterFrameworkDiscoverCoverage) {
    ParseResult r = parse({"--test-filter", "Adult.*", "--framework=doctest",
                           "--discover-cmd", "echo /tmp/a", "--allow-empty-coverage", "./t"});
    CHECK_EQ(static_cast<int>(r.kind), static_cast<int>(ParseKind::Ok));
    CHECK_EQ(r.options.test_filter, std::string("Adult.*"));
    CHECK_EQ(r.options.framework, std::string("doctest"));
    CHECK_EQ(r.options.discover_cmd, std::string("echo /tmp/a"));
    CHECK(r.options.allow_empty_coverage);
}

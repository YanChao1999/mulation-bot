#ifndef MULATION_CATALOG_HPP
#define MULATION_CATALOG_HPP

#include <cstdint>
#include <string>
#include <vector>

struct Mutant {
    uint32_t id = 0;
    std::string file;
    unsigned line = 0;
    unsigned col = 0;
    std::string kind;
    std::string op;
    std::string mut;
};

std::string json_escape(const std::string &s);
std::vector<std::string> parse_ctest_command_paths(const std::string &json);
std::vector<Mutant> parse_ndjson(const std::string &text);
std::vector<Mutant> read_elf_catalog(const std::string &binary);
std::vector<Mutant> read_catalog_dir(const std::string &dir);
std::vector<Mutant> merge_mutants(std::vector<std::vector<Mutant>> groups);

// Resolve DT_NEEDED shared libraries (via DT_RUNPATH/RPATH/$ORIGIN) that embed
// a mulation_mutants section — for SUTs linked as .so into a test binary.
std::vector<std::string> discover_linked_mutant_elfs(const std::string &binary);
// Parse stdout of --discover-cmd: one absolute/relative path per non-empty line.
std::vector<std::string> parse_discover_cmd_paths(const std::string &stdout_text);

#endif

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

std::vector<Mutant> parse_ndjson(const std::string &text);
std::vector<Mutant> read_elf_catalog(const std::string &binary);
std::vector<Mutant> read_catalog_dir(const std::string &dir);
std::vector<Mutant> merge_mutants(std::vector<std::vector<Mutant>> groups);

#endif

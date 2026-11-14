#ifndef MULATION_FILTER_HPP
#define MULATION_FILTER_HPP

#include "catalog.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct MutantFilter {
    std::vector<std::string> filter;  // keep if file contains any (OR); empty = all
    std::vector<std::string> exclude; // drop if file contains any
    std::vector<std::string> kinds;   // keep if kind matches any (case-insensitive); empty = all
    std::vector<uint32_t> mutant_ids; // keep if id matches any; empty = all
};

std::vector<Mutant> filter_mutants(const std::vector<Mutant> &mutants, const MutantFilter &f);

#endif

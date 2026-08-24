#ifndef MULATION_DIFF_HPP
#define MULATION_DIFF_HPP

#include "catalog.hpp"

#include <string>
#include <vector>

struct LineRange {
    std::string file;
    unsigned start = 0;
    unsigned end = 0;
};

std::vector<LineRange> git_diff_ranges(const std::string &base);
std::vector<Mutant> filter_by_diff(const std::vector<Mutant> &mutants,
                                   const std::vector<LineRange> &ranges);

#endif

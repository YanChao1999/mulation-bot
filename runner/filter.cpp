#include "filter.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string to_lower(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool contains_ci(const std::string &hay, const std::string &needle) {
    if (needle.empty()) {
        return true;
    }
    return to_lower(hay).find(to_lower(needle)) != std::string::npos;
}

} // namespace

std::vector<Mutant> filter_mutants(const std::vector<Mutant> &mutants, const MutantFilter &f) {
    std::vector<Mutant> out;
    out.reserve(mutants.size());
    for (const Mutant &m : mutants) {
        if (!f.filter.empty()) {
            bool ok = false;
            for (const std::string &sub : f.filter) {
                if (contains_ci(m.file, sub)) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
        }
        bool excluded = false;
        for (const std::string &sub : f.exclude) {
            if (contains_ci(m.file, sub)) {
                excluded = true;
                break;
            }
        }
        if (excluded) {
            continue;
        }
        if (!f.kinds.empty()) {
            bool ok = false;
            const std::string kind = to_lower(m.kind);
            for (const std::string &k : f.kinds) {
                if (kind == to_lower(k)) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
        }
        if (!f.mutant_ids.empty()) {
            if (std::find(f.mutant_ids.begin(), f.mutant_ids.end(), m.id) == f.mutant_ids.end()) {
                continue;
            }
        }
        out.push_back(m);
    }
    return out;
}

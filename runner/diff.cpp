#include "diff.hpp"
#include "process.hpp"

#include <cstdlib>
#include <sstream>

static bool path_match(const std::string &mut_file, const std::string &diff_file) {
    if (mut_file == diff_file) {
        return true;
    }
    if (mut_file.size() > diff_file.size()) {
        const std::size_t off = mut_file.size() - diff_file.size();
        const char sep = mut_file[off - 1];
        if ((sep == '/' || sep == '\\') &&
            mut_file.compare(off, diff_file.size(), diff_file) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<LineRange> parse_git_diff_text(const std::string &text) {
    std::vector<LineRange> ranges;
    std::string file;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("+++ b/", 0) == 0) {
            file = line.substr(6);
            continue;
        }
        if (line.rfind("@@ ", 0) != 0) {
            continue;
        }
        auto plus = line.find('+');
        if (plus == std::string::npos) {
            continue;
        }
        unsigned start = static_cast<unsigned>(std::strtoul(line.c_str() + plus + 1, nullptr, 10));
        unsigned count = 1;
        auto comma = line.find(',', plus);
        auto sp = line.find(' ', plus);
        if (comma != std::string::npos && (sp == std::string::npos || comma < sp)) {
            count = static_cast<unsigned>(std::strtoul(line.c_str() + comma + 1, nullptr, 10));
        }
        if (count == 0) {
            continue;
        }
        LineRange r;
        r.file = file;
        r.start = start;
        r.end = start + count - 1;
        ranges.push_back(std::move(r));
    }
    return ranges;
}

bool git_diff_ranges(const std::string &base, std::vector<LineRange> &out, std::string *error) {
    out.clear();
    if (!base.empty() && base != "WORKING" && base[0] == '-') {
        if (error) {
            *error = "revision must not look like a git option: " + base;
        }
        return false;
    }

    std::vector<std::string> argv = {"git", "diff", "-U0", "--no-color"};
    if (!base.empty() && base != "WORKING") {
        argv.push_back(base);
    }
    argv.emplace_back("--");

    std::string text;
    RunResult rr = run_command(argv, {}, 60000, &text);
    if (rr.status != RunStatus::Pass) {
        if (error) {
            *error = "git diff exited " + std::to_string(rr.exit_code) + " vs " + base;
        }
        return false;
    }
    out = parse_git_diff_text(text);
    return true;
}

std::vector<Mutant> filter_by_diff(const std::vector<Mutant> &mutants,
                                   const std::vector<LineRange> &ranges) {
    std::vector<Mutant> out;
    for (const Mutant &m : mutants) {
        for (const LineRange &r : ranges) {
            if (path_match(m.file, r.file) && m.line >= r.start && m.line <= r.end) {
                out.push_back(m);
                break;
            }
        }
    }
    return out;
}

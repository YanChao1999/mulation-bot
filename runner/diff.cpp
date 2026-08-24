#include "diff.hpp"

#include <cstdio>
#include <cstdlib>
#include <sstream>

static std::string run_capture(const char *cmd) {
    FILE *p = popen(cmd, "r");
    if (!p) {
        return {};
    }
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), p)) {
        out += buf;
    }
    pclose(p);
    return out;
}

static bool path_match(const std::string &mut_file, const std::string &diff_file) {
    if (mut_file == diff_file) {
        return true;
    }
    if (mut_file.size() >= diff_file.size() &&
        mut_file.compare(mut_file.size() - diff_file.size(), diff_file.size(), diff_file) == 0) {
        return true;
    }
    auto slash = [](const std::string &s) {
        auto p = s.find_last_of("/\\");
        return p == std::string::npos ? s : s.substr(p + 1);
    };
    return slash(mut_file) == slash(diff_file);
}

std::vector<LineRange> git_diff_ranges(const std::string &base) {
    std::string cmd = "git diff -U0 --no-color -- ";
    if (!base.empty() && base != "WORKING") {
        cmd = "git diff -U0 --no-color " + base + " -- ";
    }
    std::string text = run_capture(cmd.c_str());
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

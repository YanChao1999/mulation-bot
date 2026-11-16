#include "test_filter.hpp"

#include <cctype>

namespace {

std::string to_lower(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string basename_of(std::string path) {
    auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        path = path.substr(slash + 1);
    }
    return path;
}

} // namespace

std::string detect_framework(const std::vector<std::string> &cmd, const std::string &forced) {
    if (!forced.empty() && forced != "auto") {
        return to_lower(forced);
    }
    if (cmd.empty()) {
        return "gtest";
    }
    const std::string base = to_lower(basename_of(cmd[0]));
    if (base == "ctest") {
        return "ctest";
    }
    if (base == "zephyr.exe" || base == "zephyr.elf" || base.find("zephyr") == 0) {
        return "ztest";
    }
    if (base.find("doctest") != std::string::npos) {
        return "doctest";
    }
    return "gtest";
}

std::vector<std::string> native_test_filter_args(const std::string &framework,
                                                 const std::string &pattern) {
    if (pattern.empty()) {
        return {};
    }
    if (framework == "ctest") {
        return {"-R", pattern};
    }
    if (framework == "doctest") {
        return {"-tc=" + pattern};
    }
    if (framework == "ztest") {
        return {"-test=" + pattern};
    }
    if (framework == "custom") {
        return {};
    }
    // gtest (default)
    return {"--gtest_filter=" + pattern};
}

bool apply_test_filter(Options &opt, std::string *err) {
    if (opt.test_filter.empty()) {
        return true;
    }
    const std::string fw = detect_framework(opt.cmd, opt.framework);
    if (fw != "gtest" && fw != "ctest" && fw != "doctest" && fw != "ztest" && fw != "custom") {
        if (err) {
            *err = "unknown --framework '" + opt.framework +
                   "' (want auto|gtest|ctest|doctest|ztest|custom)";
        }
        return false;
    }
    opt.resolved_framework = fw;
    if (fw == "custom") {
        // Harnesses can read MULATION_TEST_FILTER; runner also exports it at exec time.
        return true;
    }
    auto args = native_test_filter_args(fw, opt.test_filter);
    opt.cmd.insert(opt.cmd.end(), args.begin(), args.end());
    return true;
}

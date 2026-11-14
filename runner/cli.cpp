#include "cli.hpp"

#include <cstdlib>
#include <sstream>

std::string usage_text(const char *argv0) {
    const char *name = argv0 ? argv0 : "mulation";
    std::ostringstream out;
    out << "mulation — mutation layer on gtest / CTest / doctest / ZTEST / custom host ELF\n"
        << "Usage:\n"
        << "  " << name << " [options] [--] <test-binary> [args...]\n"
        << "  " << name << " [options] -- ctest [ctest-args...]\n"
        << "  " << name << " c++ [clang++-args...]     (compiler wrapper)\n"
        << "  " << name << " cc  [clang-args...]       (compiler wrapper)\n"
        << "\nOptions:\n"
        << "  --min-score N       Fail if kill rate is below N percent (CI gate)\n"
        << "  --git-diff [BASE]   Only mutants on lines changed vs BASE (default HEAD)\n"
        << "  --filter SUBSTR     Keep mutants whose file contains SUBSTR (repeatable, OR)\n"
        << "  --exclude SUBSTR    Drop mutants whose file contains SUBSTR (repeatable)\n"
        << "  --kinds LIST        Comma-separated kinds to keep (e.g. ROR,AOR)\n"
        << "  --mutant-id ID      Keep only this mutant id (repeatable)\n"
        << "  --test-filter PAT   Framework-native test subset (see --framework)\n"
        << "  --framework NAME    auto|gtest|ctest|doctest|ztest|custom (default auto)\n"
        << "  --timeout-ms N      Per-mutant timeout (default: 5x baseline + 1000ms)\n"
        << "  -j, --jobs N        Parallel mutant workers (default: 1)\n"
        << "  --no-coverage       Run every mutant, not only sites hit by tests\n"
        << "  --allow-empty-coverage  If hitlog is empty, run all mutants (default: fail)\n"
        << "  --catalog-dir DIR   Also load *.ndjson catalogs from DIR\n"
        << "  --binary PATH       Extra instrumented ELF to read mutants from\n"
        << "  --discover-cmd CMD  sh -c CMD; stdout lines are extra ELF paths\n"
        << "  --json-out FILE     Write machine-readable report\n"
        << "  -h, --help          Show this help\n";
    return out.str();
}

static ParseResult err(const char *argv0, const std::string &message) {
    ParseResult r;
    r.kind = ParseKind::Error;
    r.message = message + "\n" + usage_text(argv0);
    return r;
}

static std::vector<std::string> split_csv(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (c == ' ' || c == '\t') {
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

ParseResult parse_args(int argc, char **argv) {
    ParseResult r;
    Options &o = r.options;
    const char *argv0 = (argc > 0 && argv[0]) ? argv[0] : "mulation";
    int i = 1;
    while (i < argc) {
        std::string a = argv[i];
        if (a == "--") {
            ++i;
            while (i < argc) {
                o.cmd.emplace_back(argv[i++]);
            }
            break;
        }
        if (a == "-h" || a == "--help") {
            r.kind = ParseKind::Help;
            r.message = usage_text(argv0);
            return r;
        }
        auto need = [&](const char *opt) -> bool {
            if (i + 1 >= argc || argv[i + 1] == nullptr) {
                r = err(argv0, std::string("mulation: ") + opt + " requires a value");
                return false;
            }
            return true;
        };
        if (a == "--min-score") {
            if (!need("--min-score")) {
                return r;
            }
            o.min_score = std::atoi(argv[++i]);
            o.min_score_set = true;
            ++i;
            continue;
        }
        if (a.rfind("--min-score=", 0) == 0) {
            o.min_score = std::atoi(a.c_str() + 12);
            o.min_score_set = true;
            ++i;
            continue;
        }
        if (a == "--git-diff") {
            o.git_diff = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                std::string n = argv[i + 1];
                if (n != "--" && n.find('/') == std::string::npos &&
                    n.find('.') == std::string::npos) {
                    o.git_base = n;
                    ++i;
                } else if (n == "HEAD" || n.rfind("HEAD", 0) == 0 || n.rfind("origin/", 0) == 0) {
                    o.git_base = n;
                    ++i;
                }
            }
            ++i;
            continue;
        }
        if (a.rfind("--git-diff=", 0) == 0) {
            o.git_diff = true;
            o.git_base = a.substr(11);
            ++i;
            continue;
        }
        if (a == "--filter") {
            if (!need("--filter")) {
                return r;
            }
            o.filter.emplace_back(argv[++i]);
            ++i;
            continue;
        }
        if (a.rfind("--filter=", 0) == 0) {
            o.filter.emplace_back(a.substr(9));
            ++i;
            continue;
        }
        if (a == "--exclude") {
            if (!need("--exclude")) {
                return r;
            }
            o.exclude.emplace_back(argv[++i]);
            ++i;
            continue;
        }
        if (a.rfind("--exclude=", 0) == 0) {
            o.exclude.emplace_back(a.substr(10));
            ++i;
            continue;
        }
        if (a == "--kinds") {
            if (!need("--kinds")) {
                return r;
            }
            auto parts = split_csv(argv[++i]);
            o.kinds.insert(o.kinds.end(), parts.begin(), parts.end());
            ++i;
            continue;
        }
        if (a.rfind("--kinds=", 0) == 0) {
            auto parts = split_csv(a.substr(8));
            o.kinds.insert(o.kinds.end(), parts.begin(), parts.end());
            ++i;
            continue;
        }
        if (a == "--mutant-id") {
            if (!need("--mutant-id")) {
                return r;
            }
            o.mutant_ids.push_back(static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10)));
            ++i;
            continue;
        }
        if (a.rfind("--mutant-id=", 0) == 0) {
            o.mutant_ids.push_back(static_cast<uint32_t>(std::strtoul(a.c_str() + 12, nullptr, 10)));
            ++i;
            continue;
        }
        if (a == "--test-filter") {
            if (!need("--test-filter")) {
                return r;
            }
            o.test_filter = argv[++i];
            ++i;
            continue;
        }
        if (a.rfind("--test-filter=", 0) == 0) {
            o.test_filter = a.substr(14);
            ++i;
            continue;
        }
        if (a == "--framework") {
            if (!need("--framework")) {
                return r;
            }
            o.framework = argv[++i];
            ++i;
            continue;
        }
        if (a.rfind("--framework=", 0) == 0) {
            o.framework = a.substr(12);
            ++i;
            continue;
        }
        if (a == "--timeout-ms") {
            if (!need("--timeout-ms")) {
                return r;
            }
            o.timeout_ms = std::atoi(argv[++i]);
            ++i;
            continue;
        }
        if (a == "-j" || a == "--jobs") {
            if (!need(a.c_str())) {
                return r;
            }
            o.jobs = std::atoi(argv[++i]);
            if (o.jobs < 1) {
                return err(argv0, "mulation: --jobs must be >= 1");
            }
            ++i;
            continue;
        }
        if (a.rfind("--jobs=", 0) == 0) {
            o.jobs = std::atoi(a.c_str() + 7);
            if (o.jobs < 1) {
                return err(argv0, "mulation: --jobs must be >= 1");
            }
            ++i;
            continue;
        }
        if (a == "--no-coverage") {
            o.no_coverage = true;
            ++i;
            continue;
        }
        if (a == "--allow-empty-coverage") {
            o.allow_empty_coverage = true;
            ++i;
            continue;
        }
        if (a == "--catalog-dir") {
            if (!need("--catalog-dir")) {
                return r;
            }
            o.catalog_dir = argv[++i];
            ++i;
            continue;
        }
        if (a == "--binary") {
            if (!need("--binary")) {
                return r;
            }
            o.binaries.emplace_back(argv[++i]);
            ++i;
            continue;
        }
        if (a == "--discover-cmd") {
            if (!need("--discover-cmd")) {
                return r;
            }
            o.discover_cmd = argv[++i];
            ++i;
            continue;
        }
        if (a.rfind("--discover-cmd=", 0) == 0) {
            o.discover_cmd = a.substr(15);
            ++i;
            continue;
        }
        if (a == "--json-out") {
            if (!need("--json-out")) {
                return r;
            }
            o.json_out = argv[++i];
            ++i;
            continue;
        }
        if (a.rfind("-", 0) == 0) {
            return err(argv0, "unknown option: " + a);
        }
        while (i < argc) {
            o.cmd.emplace_back(argv[i++]);
        }
        break;
    }
    return r;
}

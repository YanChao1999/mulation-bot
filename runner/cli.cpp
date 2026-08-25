#include "cli.hpp"

#include <cstdlib>
#include <sstream>

std::string usage_text(const char *argv0) {
    const char *name = argv0 ? argv0 : "mulation";
    std::ostringstream out;
    out << "mulation — mutation layer on Google Test / CTest\n"
        << "Usage:\n"
        << "  " << name << " [options] [--] <test-binary> [args...]\n"
        << "  " << name << " [options] -- ctest [ctest-args...]\n"
        << "  " << name << " c++ [clang++-args...]     (compiler wrapper)\n"
        << "  " << name << " cc  [clang-args...]       (compiler wrapper)\n"
        << "\nOptions:\n"
        << "  --min-score N       Fail if kill rate is below N percent (CI gate)\n"
        << "  --git-diff [BASE]   Only mutants on lines changed vs BASE (default HEAD)\n"
        << "  --timeout-ms N      Per-mutant timeout (default: 5x baseline + 1000ms)\n"
        << "  --no-coverage       Run every mutant, not only sites hit by tests\n"
        << "  --catalog-dir DIR   Also load *.ndjson catalogs from DIR\n"
        << "  --binary PATH       Extra instrumented ELF to read mutants from\n"
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
        if (a == "--timeout-ms") {
            if (!need("--timeout-ms")) {
                return r;
            }
            o.timeout_ms = std::atoi(argv[++i]);
            ++i;
            continue;
        }
        if (a == "--no-coverage") {
            o.no_coverage = true;
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

#include "catalog.hpp"
#include "diff.hpp"
#include "process.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <set>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

struct Options {
    int min_score = 0;
    bool min_score_set = false;
    bool git_diff = false;
    std::string git_base = "HEAD";
    int timeout_ms = 0;
    bool no_coverage = false;
    std::string catalog_dir;
    std::string json_out;
    std::vector<std::string> binaries;
    std::vector<std::string> cmd;
};

void usage(const char *argv0) {
    std::cerr << "mulation — mutation layer on Google Test / CTest\n"
              << "Usage:\n"
              << "  " << argv0 << " [options] [--] <test-binary> [args...]\n"
              << "  " << argv0 << " [options] -- ctest [ctest-args...]\n"
              << "  " << argv0 << " c++ [clang++-args...]     (compiler wrapper)\n"
              << "  " << argv0 << " cc  [clang-args...]       (compiler wrapper)\n"
              << "\nOptions:\n"
              << "  --min-score N       Fail if kill rate is below N percent (CI gate)\n"
              << "  --git-diff [BASE]   Only mutants on lines changed vs BASE (default HEAD)\n"
              << "  --timeout-ms N      Per-mutant timeout (default: 5x baseline + 1000ms)\n"
              << "  --no-coverage       Run every mutant, not only sites hit by tests\n"
              << "  --catalog-dir DIR   Also load *.ndjson catalogs from DIR\n"
              << "  --binary PATH       Extra instrumented ELF to read mutants from\n"
              << "  --json-out FILE     Write machine-readable report\n"
              << "  -h, --help          Show this help\n";
}

std::string dirname_of(std::string path) {
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    return dirname(buf.data());
}

std::string join(const std::string &a, const std::string &b) {
    if (a.empty()) {
        return b;
    }
    if (a.back() == '/') {
        return a + b;
    }
    return a + "/" + b;
}

bool file_exists(const std::string &p) {
    return access(p.c_str(), F_OK) == 0;
}

std::string find_plugin(const std::string &self) {
    if (const char *e = getenv("MULATION_PLUGIN")) {
        return e;
    }
    std::string dir = dirname_of(self);
    const char *cands[] = {"libmulation_plugin.so", "../lib/libmulation_plugin.so",
                           "../plugin/libmulation_plugin.so"};
    for (const char *c : cands) {
        std::string p = join(dir, c);
        if (file_exists(p)) {
            return p;
        }
    }
    return join(dir, "libmulation_plugin.so");
}

std::string find_runtime(const std::string &self) {
    if (const char *e = getenv("MULATION_RUNTIME")) {
        return e;
    }
    std::string dir = dirname_of(self);
    const char *cands[] = {"libmulation_runtime.a", "../lib/libmulation_runtime.a"};
    for (const char *c : cands) {
        std::string p = join(dir, c);
        if (file_exists(p)) {
            return p;
        }
    }
    return join(dir, "libmulation_runtime.a");
}

int run_wrapper(const std::string &self, const std::string &compiler,
                std::vector<std::string> args) {
    std::string plugin = find_plugin(self);
    std::string runtime = find_runtime(self);
    bool compile_only = false;
    bool is_link = true;
    for (const auto &a : args) {
        if (a == "-c" || a == "-S" || a == "-E" || a == "-fsyntax-only") {
            compile_only = true;
            is_link = false;
        }
    }
    std::vector<std::string> cmd;
    cmd.push_back(compiler);
    cmd.push_back("-g");
    cmd.push_back("-fpass-plugin=" + plugin);
    cmd.insert(cmd.end(), args.begin(), args.end());
    if (is_link && !compile_only) {
        cmd.push_back(runtime);
        cmd.push_back("-Wl,--export-dynamic");
    }
    std::vector<char *> cargv;
    for (auto &a : cmd) {
        cargv.push_back(a.data());
    }
    cargv.push_back(nullptr);
    execvp(cargv[0], cargv.data());
    std::cerr << "mulation: failed to exec " << compiler << "\n";
    return 127;
}

bool looks_like_compiler(const std::string &s) {
    auto base = s;
    auto slash = base.find_last_of("/\\");
    if (slash != std::string::npos) {
        base = base.substr(slash + 1);
    }
    return base == "c++" || base == "cc" || base == "clang" || base == "clang++" || base == "g++" ||
           base == "gcc";
}

std::string compiler_for(const std::string &alias) {
    if (alias == "c++" || alias == "clang++" || alias == "g++") {
        return "clang++";
    }
    return "clang";
}

Options parse_args(int argc, char **argv) {
    Options o;
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
            usage(argv[0]);
            std::exit(0);
        }
        if (a == "--min-score") {
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
            o.catalog_dir = argv[++i];
            ++i;
            continue;
        }
        if (a == "--binary") {
            o.binaries.emplace_back(argv[++i]);
            ++i;
            continue;
        }
        if (a == "--json-out") {
            o.json_out = argv[++i];
            ++i;
            continue;
        }
        if (a.rfind("-", 0) == 0) {
            std::cerr << "unknown option: " << a << "\n";
            usage(argv[0]);
            std::exit(2);
        }
        while (i < argc) {
            o.cmd.emplace_back(argv[i++]);
        }
        break;
    }
    return o;
}

std::string suggestion(const Mutant &m) {
    if (m.kind == "ROR") {
        return "add a boundary case that distinguishes `" + m.op + "` from `" + m.mut + "`";
    }
    if (m.kind == "AOR") {
        return "assert the exact arithmetic result (not only a smoke value)";
    }
    if (m.kind == "LCR") {
        return "add a case where `&&` and `||` disagree";
    }
    if (m.kind == "LVR") {
        return "cover the zero/one literal independently";
    }
    return "add an assertion that would fail if this operator changed";
}

std::set<uint32_t> load_hitlog(const std::string &path) {
    std::set<uint32_t> ids;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            ids.insert(static_cast<uint32_t>(std::strtoul(line.c_str(), nullptr, 10)));
        }
    }
    return ids;
}

bool is_elf(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    char mag[4]{};
    in.read(mag, 4);
    return in && mag[0] == 0x7f && mag[1] == 'E' && mag[2] == 'L' && mag[3] == 'F';
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    std::string self = argv[0];
    if (argc >= 2 && looks_like_compiler(argv[1])) {
        std::vector<std::string> rest;
        for (int i = 2; i < argc; ++i) {
            rest.emplace_back(argv[i]);
        }
        return run_wrapper(self, compiler_for(argv[1]), rest);
    }

    Options opt = parse_args(argc, argv);
    if (opt.cmd.empty()) {
        usage(argv[0]);
        return 2;
    }

    std::vector<std::string> elfs;
    std::vector<std::vector<Mutant>> groups;
    auto add_elf = [&](const std::string &p) {
        if (is_elf(p)) {
            elfs.push_back(p);
        }
    };
    add_elf(opt.cmd[0]);
    for (const auto &b : opt.binaries) {
        add_elf(b);
    }
    for (const auto &p : elfs) {
        groups.push_back(read_elf_catalog(p));
    }
    if (!opt.catalog_dir.empty()) {
        groups.push_back(read_catalog_dir(opt.catalog_dir));
    } else if (const char *e = getenv("MULATION_CATALOG_DIR")) {
        groups.push_back(read_catalog_dir(e));
    }

    std::vector<Mutant> mutants = merge_mutants(std::move(groups));
    if (mutants.empty()) {
        std::cerr << "mulation: no mutants found. Rebuild the SUT with:\n"
                  << "  CXX=\"" << self << " c++\"  (or -fpass-plugin=libmulation_plugin.so -g)\n"
                  << "and link libmulation_runtime.a\n";
        return 2;
    }

    if (opt.git_diff) {
        auto ranges = git_diff_ranges(opt.git_base);
        mutants = filter_by_diff(mutants, ranges);
        if (mutants.empty()) {
            std::cout << "mulation: no mutants on git diff vs " << opt.git_base
                      << "; nothing to prove.\n";
            return 0;
        }
    }

    std::cout << "mulation: baseline (mutants off)...\n";
    RunResult base = run_command(opt.cmd, {{"MULATION_MUTANT", ""}},
                                 opt.timeout_ms > 0 ? opt.timeout_ms : 600000);
    if (base.status != RunStatus::Pass) {
        std::cerr << "mulation: original suite is not green (exit " << base.exit_code
                  << "). Mutation is meaningless until tests pass.\n";
        return 1;
    }

    int timeout = opt.timeout_ms;
    if (timeout <= 0) {
        timeout = std::max(2000, base.duration_ms * 5 + 1000);
    }

    std::set<uint32_t> covered;
    if (!opt.no_coverage) {
        char tmp[] = "/tmp/mulation-hits-XXXXXX";
        int fd = mkstemp(tmp);
        if (fd >= 0) {
            close(fd);
            run_command(opt.cmd, {{"MULATION_MUTANT", ""}, {"MULATION_HITLOG", tmp}}, timeout);
            covered = load_hitlog(tmp);
            unlink(tmp);
        }
    }

    int killed = 0, survived = 0, timeout_n = 0;
    std::vector<Mutant> survivors;
    std::vector<Mutant> uncovered;

    std::cout << "mulation: " << mutants.size() << " mutants";
    if (!covered.empty()) {
        std::cout << ", " << covered.size() << " sites hit by tests";
    }
    std::cout << "\n";

    int idx = 0;
    for (const Mutant &m : mutants) {
        ++idx;
        if (!opt.no_coverage && !covered.empty() && !covered.count(m.id)) {
            uncovered.push_back(m);
            continue;
        }
        std::map<std::string, std::string> env{{"MULATION_MUTANT", std::to_string(m.id)}};
        RunResult rr = run_command(opt.cmd, env, timeout);
        const char *cls = "survived";
        if (rr.status == RunStatus::Timeout) {
            cls = "timeout";
            ++timeout_n;
            ++killed;
        } else if (rr.status == RunStatus::Pass) {
            cls = "survived";
            ++survived;
            survivors.push_back(m);
        } else {
            cls = "killed";
            ++killed;
        }
        std::cout << "  [" << idx << "/" << mutants.size() << "] " << cls << "  " << m.file << ":"
                  << m.line << "  [" << m.kind << "] " << m.op << " -> " << m.mut << "\n";
    }

    survived += static_cast<int>(uncovered.size());
    const int denom = killed + survived;
    const double score = denom == 0 ? 100.0 : (100.0 * killed / denom);

    std::cout << "\nMulation report\n";
    std::cout << "  mutants:   " << mutants.size() << "\n";
    std::cout << "  killed:    " << killed << "\n";
    std::cout << "  survived:  " << survived << "\n";
    std::cout << "  timeout:   " << timeout_n << " (counted as killed)\n";
    if (!uncovered.empty()) {
        std::cout << "  uncovered: " << uncovered.size() << " (counted as survived)\n";
    }
    std::cout << "  score:     " << score << "%\n";

    if (!survivors.empty() || !uncovered.empty()) {
        std::cout << "\nSurvived (tests would miss this bug in production):\n";
        auto dump = [](const Mutant &m, const char *tag) {
            std::cout << "  " << m.file << ":" << m.line << ":" << m.col << "  [" << m.kind << "] `"
                      << m.op << "` -> `" << m.mut << "`";
            if (tag) {
                std::cout << "  " << tag;
            }
            std::cout << "\n    suggestion: " << suggestion(m) << "\n";
        };
        for (const Mutant &m : survivors) {
            dump(m, nullptr);
        }
        for (const Mutant &m : uncovered) {
            dump(m, "[not covered]");
        }
    }

    if (!opt.json_out.empty()) {
        std::ofstream js(opt.json_out);
        js << "{\"killed\":" << killed << ",\"survived\":" << survived
           << ",\"timeout\":" << timeout_n << ",\"score\":" << score << ",\"survivors\":[";
        bool first = true;
        auto emit = [&](const Mutant &m) {
            if (!first) {
                js << ",";
            }
            first = false;
            js << "{\"id\":" << m.id << ",\"file\":\"" << m.file << "\",\"line\":" << m.line
               << ",\"kind\":\"" << m.kind << "\",\"op\":\"" << m.op << "\",\"mut\":\"" << m.mut
               << "\"}";
        };
        for (const Mutant &m : survivors) {
            emit(m);
        }
        for (const Mutant &m : uncovered) {
            emit(m);
        }
        js << "]}\n";
    }

    if (opt.min_score_set && score + 1e-9 < opt.min_score) {
        std::cerr << "mulation: score " << score << "% is below --min-score " << opt.min_score
                  << "\n";
        return 1;
    }
    return 0;
}

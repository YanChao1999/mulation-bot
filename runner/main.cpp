#include "catalog.hpp"
#include "cli.hpp"
#include "diff.hpp"
#include "process.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

std::string dirname_of(std::string path) {
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    return dirname(buf.data());
}

std::string basename_of(std::string path) {
    auto slash = path.find_last_of("/\\");
    if (slash != std::string::npos) {
        path = path.substr(slash + 1);
    }
    return path;
}

std::string executable_path(const char *argv0) {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return std::string(buf, static_cast<std::size_t>(n));
    }
    return argv0 ? argv0 : "";
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
    auto base = basename_of(s);
    return base == "c++" || base == "cc" || base == "clang" || base == "clang++" || base == "g++" ||
           base == "gcc";
}

std::string compiler_for(const std::string &alias) {
    if (alias == "c++" || alias == "clang++" || alias == "g++") {
        if (const char *e = getenv("CXX")) {
            return e;
        }
        return "clang++";
    }
    if (const char *e = getenv("CC")) {
        return e;
    }
    return "clang";
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

std::vector<std::string> discover_ctest_binaries(const std::vector<std::string> &cmd) {
    std::vector<std::string> show = cmd;
    show.insert(show.begin() + 1, "--show-only=json-v1");
    std::string json;
    RunResult rr = run_command(show, {}, 60000, &json);
    if (rr.status != RunStatus::Pass) {
        return {};
    }
    return parse_ctest_command_paths(json);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << usage_text(argv[0]);
        return 2;
    }

    std::string self = executable_path(argv[0]);
    if (argc >= 2 && looks_like_compiler(argv[1])) {
        std::vector<std::string> rest;
        for (int i = 2; i < argc; ++i) {
            rest.emplace_back(argv[i]);
        }
        return run_wrapper(self, compiler_for(argv[1]), rest);
    }

    ParseResult parsed = parse_args(argc, argv);
    if (parsed.kind == ParseKind::Help) {
        std::cout << parsed.message;
        return 0;
    }
    if (parsed.kind == ParseKind::Error) {
        std::cerr << parsed.message;
        return 2;
    }
    Options opt = std::move(parsed.options);
    if (opt.cmd.empty()) {
        std::cerr << usage_text(argv[0]);
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
    if (basename_of(opt.cmd[0]) == "ctest") {
        for (const auto &p : discover_ctest_binaries(opt.cmd)) {
            add_elf(p);
        }
    }
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
        if (basename_of(opt.cmd[0]) == "ctest") {
            std::cerr
                << "For `mulation -- ctest`, test binaries are discovered via "
                   "`ctest --show-only=json-v1`; you can also pass --binary or --catalog-dir.\n";
        }
        return 2;
    }

    if (opt.git_diff) {
        std::vector<LineRange> ranges;
        std::string git_err;
        if (!git_diff_ranges(opt.git_base, ranges, &git_err)) {
            std::cerr << "mulation: " << git_err << "\n";
            return 2;
        }
        mutants = filter_by_diff(mutants, ranges);
        if (mutants.empty()) {
            std::cout << "mulation: no mutants on git diff vs " << opt.git_base
                      << "; nothing to prove.\n";
            if (!opt.json_out.empty()) {
                std::ofstream js(opt.json_out);
                js << "{\"killed\":0,\"survived\":0,\"timeout\":0,\"score\":100,\"survivors\":[]}"
                      "\n";
            }
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
        // Keep hangs from dominating wall time; 3x baseline is enough for gtest suites.
        timeout = std::max(1500, base.duration_ms * 3 + 500);
    }

    std::set<uint32_t> covered;
    if (!opt.no_coverage) {
        char tmp[] = "/tmp/mulation-hits-XXXXXX";
        int fd = mkstemp(tmp);
        if (fd >= 0) {
            close(fd);
            RunResult cov =
                run_command(opt.cmd, {{"MULATION_MUTANT", ""}, {"MULATION_HITLOG", tmp}}, timeout);
            if (cov.status != RunStatus::Pass) {
                std::cerr << "mulation: coverage baseline failed (exit " << cov.exit_code
                          << "); refusing a partial hit log.\n";
                unlink(tmp);
                return 1;
            }
            covered = load_hitlog(tmp);
            unlink(tmp);
        }
    }

    int killed = 0, survived = 0, timeout_n = 0;
    std::vector<Mutant> survivors;
    std::vector<Mutant> uncovered;
    std::vector<const Mutant *> todo;
    todo.reserve(mutants.size());

    std::cout << "mulation: " << mutants.size() << " mutants";
    if (!covered.empty()) {
        std::cout << ", " << covered.size() << " sites hit by tests";
    }
    const int jobs = std::max(1, opt.jobs);
    if (jobs > 1) {
        std::cout << ", jobs=" << jobs;
    }
    std::cout << "\n";

    for (const Mutant &m : mutants) {
        if (!opt.no_coverage && !covered.empty() && !covered.count(m.id)) {
            uncovered.push_back(m);
            continue;
        }
        todo.push_back(&m);
    }

    struct Outcome {
        size_t order = 0;
        const Mutant *m = nullptr;
        const char *cls = "survived";
        bool is_killed = false;
        bool is_timeout = false;
        bool is_survived = false;
    };
    std::vector<Outcome> outcomes(todo.size());
    std::atomic<size_t> next{0};
    std::mutex print_mu;

    auto worker = [&]() {
        while (true) {
            size_t i = next.fetch_add(1);
            if (i >= todo.size()) {
                break;
            }
            const Mutant *m = todo[i];
            std::map<std::string, std::string> env{{"MULATION_MUTANT", std::to_string(m->id)}};
            RunResult rr = run_command(opt.cmd, env, timeout);
            Outcome o;
            o.order = i;
            o.m = m;
            if (rr.status == RunStatus::Timeout) {
                o.cls = "timeout";
                o.is_timeout = true;
                o.is_killed = true;
            } else if (rr.status == RunStatus::Pass) {
                o.cls = "survived";
                o.is_survived = true;
            } else {
                o.cls = "killed";
                o.is_killed = true;
            }
            outcomes[i] = o;
            std::lock_guard<std::mutex> lock(print_mu);
            std::cout << "  [" << (i + 1) << "/" << todo.size() << "] " << o.cls << "  " << m->file
                      << ":" << m->line << "  [" << m->kind << "] " << m->op << " -> " << m->mut
                      << "\n";
        }
    };

    if (jobs == 1 || todo.size() <= 1) {
        worker();
    } else {
        const size_t n = std::min(static_cast<size_t>(jobs), todo.size());
        std::vector<std::thread> pool;
        pool.reserve(n);
        for (size_t t = 0; t < n; ++t) {
            pool.emplace_back(worker);
        }
        for (auto &th : pool) {
            th.join();
        }
    }

    for (const Outcome &o : outcomes) {
        if (!o.m) {
            continue;
        }
        if (o.is_timeout) {
            ++timeout_n;
        }
        if (o.is_killed) {
            ++killed;
        }
        if (o.is_survived) {
            ++survived;
            survivors.push_back(*o.m);
        }
    }

    survived += static_cast<int>(uncovered.size());
    // Timeouts are reported but excluded from the score: hang-prone loop mutants
    // otherwise inflate "killed" without proving an assertion caught the fault.
    const int killed_scored = killed - timeout_n;
    const int denom = killed_scored + survived;
    const double score = denom == 0 ? 100.0 : (100.0 * killed_scored / denom);

    std::cout << "\nMulation report\n";
    std::cout << "  mutants:   " << mutants.size() << "\n";
    std::cout << "  killed:    " << killed_scored << "\n";
    std::cout << "  survived:  " << survived << "\n";
    std::cout << "  timeout:   " << timeout_n << " (excluded from score)\n";
    if (!uncovered.empty()) {
        std::cout << "  uncovered: " << uncovered.size() << " (counted as survived)\n";
    }
    std::cout << "  score:     " << score << "%\n";

    if (!survivors.empty() || !uncovered.empty()) {
        std::cout << "\nSurvived:\n";
        auto dump = [](const Mutant &m, const char *tag) {
            std::cout << "  " << m.file << ":" << m.line << ":" << m.col << "  [" << m.kind << "] `"
                      << m.op << "` -> `" << m.mut << "`";
            if (tag) {
                std::cout << "  " << tag;
            }
            std::cout << "\n";
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
        js << "{\"killed\":" << killed_scored << ",\"survived\":" << survived
           << ",\"timeout\":" << timeout_n << ",\"score\":" << score << ",\"survivors\":[";
        bool first = true;
        auto emit = [&](const Mutant &m) {
            if (!first) {
                js << ",";
            }
            first = false;
            js << "{\"id\":" << m.id << ",\"file\":\"" << json_escape(m.file)
               << "\",\"line\":" << m.line << ",\"kind\":\"" << json_escape(m.kind)
               << "\",\"op\":\"" << json_escape(m.op) << "\",\"mut\":\"" << json_escape(m.mut)
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

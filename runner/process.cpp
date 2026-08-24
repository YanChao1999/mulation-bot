#include "process.hpp"

#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

RunResult run_command(const std::vector<std::string> &argv,
                      const std::map<std::string, std::string> &extra_env, int timeout_ms) {
    RunResult r;
    if (argv.empty()) {
        return r;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return r;
    }
    if (pid == 0) {
        for (const auto &kv : extra_env) {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            dup2(dn, STDOUT_FILENO);
            dup2(dn, STDERR_FILENO);
            if (dn > 2) {
                close(dn);
            }
        }
        std::vector<char *> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto &a : argv) {
            cargv.push_back(const_cast<char *>(a.c_str()));
        }
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto deadline = start + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 600000);

    int status = 0;
    while (true) {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            break;
        }
        if (clock::now() >= deadline) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            r.status = RunStatus::Timeout;
            r.exit_code = 124;
            r.duration_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count());
            return r;
        }
        usleep(5000);
    }

    r.duration_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count());
    if (WIFEXITED(status)) {
        r.exit_code = WEXITSTATUS(status);
        r.status = (r.exit_code == 0) ? RunStatus::Pass : RunStatus::Fail;
    } else if (WIFSIGNALED(status)) {
        r.exit_code = 128 + WTERMSIG(status);
        r.status = RunStatus::Crash;
    }
    return r;
}

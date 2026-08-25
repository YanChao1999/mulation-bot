#include "process.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

static void drain_fd(int fd, std::string *out) {
    if (fd < 0 || !out) {
        return;
    }
    char buf[4096];
    while (true) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            out->append(buf, static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }
        break;
    }
}

RunResult run_command(const std::vector<std::string> &argv,
                      const std::map<std::string, std::string> &extra_env, int timeout_ms,
                      std::string *stdout_out) {
    RunResult r;
    if (argv.empty()) {
        return r;
    }

    int outpipe[2] = {-1, -1};
    if (stdout_out) {
        if (pipe(outpipe) != 0) {
            return r;
        }
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (outpipe[0] >= 0) {
            close(outpipe[0]);
            close(outpipe[1]);
        }
        return r;
    }
    if (pid == 0) {
        setpgid(0, 0);
        if (stdout_out) {
            close(outpipe[0]);
            if (dup2(outpipe[1], STDOUT_FILENO) < 0) {
                _exit(127);
            }
            if (outpipe[1] != STDOUT_FILENO) {
                close(outpipe[1]);
            }
        }
        for (const auto &kv : extra_env) {
            setenv(kv.first.c_str(), kv.second.c_str(), 1);
        }
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) {
            if (!stdout_out) {
                dup2(dn, STDOUT_FILENO);
            }
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

    setpgid(pid, pid);

    if (stdout_out) {
        close(outpipe[1]);
        int flags = fcntl(outpipe[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(outpipe[0], F_SETFL, flags | O_NONBLOCK);
        }
    }

    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    const auto deadline = start + std::chrono::milliseconds(timeout_ms > 0 ? timeout_ms : 600000);

    int status = 0;
    bool timed_out = false;
    while (true) {
        drain_fd(outpipe[0], stdout_out);
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            break;
        }
        if (clock::now() >= deadline) {
            timed_out = true;
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(5000);
    }

    if (outpipe[0] >= 0) {
        int flags = fcntl(outpipe[0], F_GETFL, 0);
        if (flags >= 0) {
            fcntl(outpipe[0], F_SETFL, flags & ~O_NONBLOCK);
        }
        drain_fd(outpipe[0], stdout_out);
        close(outpipe[0]);
    }

    r.duration_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count());
    if (timed_out) {
        r.status = RunStatus::Timeout;
        r.exit_code = 124;
        return r;
    }
    if (WIFEXITED(status)) {
        r.exit_code = WEXITSTATUS(status);
        r.status = (r.exit_code == 0) ? RunStatus::Pass : RunStatus::Fail;
    } else if (WIFSIGNALED(status)) {
        r.exit_code = 128 + WTERMSIG(status);
        r.status = RunStatus::Crash;
    }
    return r;
}

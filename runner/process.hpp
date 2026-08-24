#ifndef MULATION_PROCESS_HPP
#define MULATION_PROCESS_HPP

#include <map>
#include <string>
#include <vector>

enum class RunStatus { Pass, Fail, Timeout, Crash };

struct RunResult {
    RunStatus status = RunStatus::Fail;
    int exit_code = 1;
    int duration_ms = 0;
};

RunResult run_command(const std::vector<std::string> &argv,
                      const std::map<std::string, std::string> &extra_env, int timeout_ms,
                      std::string *stdout_out = nullptr);

#endif

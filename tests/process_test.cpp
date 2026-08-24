#include "process.hpp"
#include "unit_check.hpp"

TEST(Process, TruePasses) {
    RunResult r = run_command({"/bin/true"}, {}, 2000);
    CHECK_EQ(static_cast<int>(r.status), static_cast<int>(RunStatus::Pass));
    CHECK_EQ(r.exit_code, 0);
}

TEST(Process, FalseFails) {
    RunResult r = run_command({"/bin/false"}, {}, 2000);
    CHECK_EQ(static_cast<int>(r.status), static_cast<int>(RunStatus::Fail));
    CHECK(r.exit_code != 0);
}

TEST(Process, TimeoutKillsSleep) {
    RunResult r = run_command({"/bin/sleep", "5"}, {}, 80);
    CHECK_EQ(static_cast<int>(r.status), static_cast<int>(RunStatus::Timeout));
    CHECK_EQ(r.exit_code, 124);
}

TEST(Process, EmptyArgvFails) {
    RunResult r = run_command({}, {}, 1000);
    CHECK_EQ(static_cast<int>(r.status), static_cast<int>(RunStatus::Fail));
}

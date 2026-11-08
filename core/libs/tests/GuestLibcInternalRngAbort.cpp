#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal _Rng_abort: %s\n", message);
    std::abort();
}

struct Outcome {
    int status;
    std::string output;
};

Outcome RunAbort(const char* message) {
    int pipeFds[2];
    Require(pipe(pipeFds) == 0, "pipe");
    const pid_t child = fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        const rlimit noCore{0, 0};
        setrlimit(RLIMIT_CORE, &noCore);
        close(pipeFds[0]);
        dup2(pipeFds[1], 2);
        close(pipeFds[1]);
        sceLibcInternalRngAbort_nid_no_patch(message);
    }
    close(pipeFds[1]);
    Outcome outcome{0, {}};
    char buffer[256];
    for (ssize_t count; (count = read(pipeFds[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(count));
    close(pipeFds[0]);
    Require(waitpid(child, &outcome.status, 0) == child, "waitpid");
    return outcome;
}

}

int main() {
    const Outcome outcome = RunAbort("invalid argument for uniform_int_distribution");
    Require(WIFSIGNALED(outcome.status) && WTERMSIG(outcome.status) == SIGABRT, "_Rng_abort ends the process with SIGABRT");
    Require(outcome.output.rfind("invalid argument for uniform_int_distribution\n", 0) == 0, "_Rng_abort writes the message and a newline to stderr before aborting");

    const Outcome empty = RunAbort("");
    Require(WIFSIGNALED(empty.status) && WTERMSIG(empty.status) == SIGABRT, "_Rng_abort with an empty message aborts");
    Require(empty.output.rfind("\n", 0) == 0, "_Rng_abort with an empty message writes only the newline first");
    std::puts("libc internal _Rng_abort: ok");
    return 0;
}

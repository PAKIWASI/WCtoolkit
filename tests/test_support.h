#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

// Shared by every test file: the utest framework plus EXPECT_DIES.
//
// utest.h  https://github.com/sheredom/utest.h  (public domain), vendored as-is.

#include "utest.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Runs fn() in a forked child with its output silenced.
// Returns 1 if the child died (non-zero exit or signal), 0 if it returned.
static inline int test_dies(void (*fn)(void))
{
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            dup2(devnull, STDOUT_FILENO);
        }
        fn();
        _exit(0); // survived: skip atexit / leak checks so 0 really means "did not die"
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    return WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0);
}

// Death tests are available (this header already requires POSIX fork()).
#define WC_HAS_FORK 1

// Expect fn() to abort (FATAL_IF, WC_ASSERT in Debug, ...). POSIX only.
#define EXPECT_DIES(fn) EXPECT_EQ_MSG(test_dies(fn), 1, #fn " should terminate")

#endif // TEST_SUPPORT_H

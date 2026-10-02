#ifndef WC_TEST_FATAL_H
#define WC_TEST_FATAL_H

/*
 * WC_EXPECT_DIES(fn): assert that calling `fn` (void fn(void)) terminates the
 * process through FATAL/abort, without taking the test runner down with it.
 *
 * Runs `fn` in a forked child with stderr silenced. "Died" means any non-zero
 * exit status or a signal. (FATAL exits with EXIT_FAILURE; under ASAN the leak
 * checker may change the status at exit, which still counts as died.)
 *
 * POSIX only. Debug-only checks (WC_ASSERT) vanish under NDEBUG, so guard
 * those uses with #ifndef NDEBUG.
 */

#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static inline int wc_test_dies(void (*fn)(void))
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
        _exit(0); // survived: skip atexit/LSan so "0" really means "did not die"
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    if (WIFSIGNALED(status)) {
        return 1;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) != 0;
}

#define WC_EXPECT_DIES(fn) WC_EXPECT_CORE(wc_test_dies(fn) == 1, #fn " terminates via FATAL")

#endif // WC_TEST_FATAL_H

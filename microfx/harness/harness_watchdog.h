// harness_watchdog.h — SIGALRM watchdog for the microfx smoke harnesses.
//
// A smoke test that spins (e.g. a the engine retry loop stuck at 100% CPU on a debug-pinned
// seed, or any future infinite loop) is a SILENT failure: the harness never prints its PASS
// line, the build script just stalls. Arm this once at the top of main() and any such hang
// becomes a loud, non-zero exit with a best-effort backtrace — no eyes on the terminal needed.
//
// Usage:
//     #include "harness_watchdog.h"
//     int main(...) {
//         HarnessArmWatchdog(60);   // kill with a stack trace if main runs > 60s (SIGALRM)
//         ...
//     }
//
// Note: SIGALRM is delivered on the thin main thread only; pure compute in main() is what we
// are guarding, so that is exactly where it lands. Runs on macOS + Linux (alarm()/SIGALRM).

#ifndef HARNESS_WATCHDOG_H
#define HARNESS_WATCHDOG_H

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(__APPLE__) || defined(__linux__)
#include <execinfo.h>            // backtrace_symbols_fd (best-effort in the handler)
#endif

static void HarnessWatchdogAlarm(int /*sig*/)
{
#if defined(__APPLE__) || defined(__linux__)
    void* buf[32];
    int n = backtrace(buf, 32);          // technically not async-signal-safe, but works for this
    fputs("HARNESS WATCHDOG: main() exceeded the time budget — probable infinite loop.\n", stderr);
    fputs("HARNESS WATCHDOG: call stack:\n", stderr);
    backtrace_symbols_fd(buf, n, STDERR_FILENO);
#else
    fputs("HARNESS WATCHDOG: main() exceeded the time budget — probable infinite loop.\n", stderr);
#endif
    _Exit(1);                            // loud non-zero failure; no atexit cleanup needed here
}

static inline void HarnessArmWatchdog(unsigned seconds)
{
    signal(SIGALRM, HarnessWatchdogAlarm);
    alarm(seconds);
}

#endif // HARNESS_WATCHDOG_H

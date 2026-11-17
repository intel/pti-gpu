# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

"""CPU regression for calibration between different host clock domains."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


_DRIVER = r'''
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <sys/syscall.h>
#include <unistd.h>

#include "src/unitimer.h"

// Keep wall-clock reads real, but make the host domains differ by 30 seconds.
extern "C" int clock_gettime(clockid_t clock, timespec* ts) noexcept {
    switch (clock) {
        case CLOCK_MONOTONIC_RAW: ts->tv_sec = 10; break;
        case CLOCK_BOOTTIME: ts->tv_sec = 40; break;
        case CLOCK_TAI: ts->tv_sec = 100; break;
        default: return syscall(SYS_clock_gettime, clock, ts);
    }
    ts->tv_nsec = 0;
    return 0;
}

int main() {
    auto before = std::chrono::system_clock::now();
    UniTimer::StartUniTimer();
    auto epoch = UniTimer::GetEpochTime(UniTimer::GetHostTimestamp());
    auto after = std::chrono::system_clock::now();
    auto ns = [](auto time) {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            time.time_since_epoch()).count();
    };
    std::printf("%lld %llu %lld\n", static_cast<long long>(ns(before)),
                static_cast<unsigned long long>(epoch),
                static_cast<long long>(ns(after)));
    return 0;
}
'''


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux clock domains")
class UniTimerTest(unittest.TestCase):
    def test_epoch_calibration_uses_trace_clock(self):
        compiler = shutil.which("g++")
        if compiler is None:
            self.skipTest("g++ is unavailable")
        unitrace = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory, "clock.cc")
            binary = Path(directory, "clock")
            source.write_text(_DRIVER)
            build = subprocess.run(
                [compiler, "-std=c++17", "-I", str(unitrace), str(source),
                 "-o", str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(binary)], capture_output=True,
                                 text=True, timeout=10)
            self.assertEqual(run.returncode, 0, run.stderr)
            before, epoch, after = map(int, run.stdout.split())
            self.assertLessEqual(before, epoch)
            self.assertLessEqual(epoch, after)


if __name__ == "__main__":
    unittest.main()

//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UNITIMER_H
#define PTI_TOOLS_UNITRACE_UNITIMER_H

#include "utils.h"
#include <chrono>
#include <iostream>

class UniTimer {
  public:
    static void StartUniTimer(void) {
        if (epoch_start_time_ == 0) {
            // loop multiple times to mitigate context switch impact
            for (int i = 0; i < 100; i++) {
                uint64_t start;
                const std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
                start = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count() - GetHostTimestamp();
                if (start > epoch_start_time_) {
                    epoch_start_time_ = start;
                }
            }
        }
    }

    static uint64_t GetEpochTime(uint64_t systime) {
        return epoch_start_time_ + systime;
    }

    static uint64_t GetEpochTimeInUs(uint64_t systime) {
        return ((epoch_start_time_ + systime) / 1000);
    }
        
    static uint64_t GetTimeInUs(uint64_t systime) {
        return (systime / 1000);
    }
        
    static uint64_t GetHostTimestamp() {
#if defined(_WIN32)
        LARGE_INTEGER ticks{0};
        LARGE_INTEGER frequency{0};
        BOOL status = QueryPerformanceFrequency(&frequency);
        status = QueryPerformanceCounter(&ticks);
        PTI_ASSERT(status != 0);
        return ticks.QuadPart * (NSEC_IN_SEC / frequency.QuadPart);
#else /* _WIN32 */
        timespec ts{0};
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return ts.tv_sec * NSEC_IN_SEC + ts.tv_nsec;
#endif /* _WIN32 */
    }
  private:
    inline static uint64_t epoch_start_time_ = 0;
};
    
#endif // PTI_TOOLS_UNITRACE_UNITIMER_H

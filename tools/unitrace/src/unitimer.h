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
#if defined(_WIN32)
        if (frequency_.QuadPart == 0) {
          if (!QueryPerformanceFrequency(&frequency_)) {
            std::cerr << "[ERROR] Failed to query performance counter frequency" << std::endl;
            exit(-1);
          }
        }
#endif /* _WIN32 */
        if ((utils::GetEnv("UNITRACE_SystemTime") != "1") && (epoch_start_time_ == 0)) {
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
    
    static double GetEpochTimeInUs(uint64_t systime) {
        // (double(1.0) * (epoch_start_time_ + systime) / 1000.0);
        uint64_t us = (epoch_start_time_ + systime) / 1000;
        uint64_t ns = (epoch_start_time_ + systime) % 1000;
        return double(us) + (double(ns) * 0.001);
    }
        
    static double GetTimeInUs(uint64_t systime) {
        // (double(1.0) * systime / 1000.0);
        uint64_t us = systime / 1000;
        uint64_t ns = systime % 1000;
        return double(us) + (double(ns) * 0.001);
    }
        
    static uint64_t GetHostTimestamp() {
#if defined(_WIN32)
        LARGE_INTEGER ticks;
        if (!QueryPerformanceCounter(&ticks)) {
          std::cerr << "[ERROR] Failed to query performance counter" << std::endl;
          exit(-1);
        }
        return ticks.QuadPart * (NSEC_IN_SEC / frequency_.QuadPart);
#else /* _WIN32 */
        timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts)) {
          std::cerr << "[ERROR] Failed to get timestamp" << std::endl;
          exit(-1);
        }
        return ts.tv_sec * NSEC_IN_SEC + ts.tv_nsec;
#endif /* _WIN32 */
    }
private:
    inline static uint64_t epoch_start_time_ = 0;
#if defined(_WIN32)
    inline static LARGE_INTEGER frequency_{{0}};
#endif /* _WIN32 */
};
    
#endif // PTI_TOOLS_UNITRACE_UNITIMER_H

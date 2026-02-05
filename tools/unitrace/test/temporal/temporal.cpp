//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <sycl/sycl.hpp>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

using namespace sycl;
int main(int argc, char *argv[]) {
    std::string session;
    std::string unitrace_path;
    auto session_env = std::getenv("UNITRACE_Session");
    if (session_env != nullptr) {
        session = session_env;
    }
#ifndef _WIN32
    auto preload_env = std::getenv("LD_PRELOAD");
    if (preload_env != nullptr) {
        unitrace_path = preload_env;
        auto pos = unitrace_path.find("libunitrace_tool");
        if (pos != std::string::npos) {
            unitrace_path.replace(pos, sizeof("libunitrace_tool.so"), "unitrace"); 
            unsetenv("LD_PRELOAD");
        }
    }
#else /* _WIN32 */
    HMODULE hModule = GetModuleHandleW(L"unitrace_tool.dll");
    if (hModule != nullptr) {
        wchar_t module_path[MAX_PATH];
        if (GetModuleFileNameW(hModule, module_path, MAX_PATH) > 0) {
            std::wstring wide_path(module_path);
            std::string path(wide_path.begin(), wide_path.end());
            auto pos = path.find_last_of("\\/");
            if (pos != std::string::npos) {
                unitrace_path = path.substr(0, pos + 1) + "unitrace.exe";
            }
        }
    }
#endif /* _WIN32 */
    int GLOBAL_SIZE = 64 * 512;
    queue q{gpu_selector(), {property::queue::in_order()}};
    float *a = (float *)malloc_shared(sizeof(float)*GLOBAL_SIZE,q);
    float *b = (float *)malloc_shared(sizeof(float)*GLOBAL_SIZE,q);
    float *c = (float *)malloc_shared(sizeof(float)*GLOBAL_SIZE,q);
    for(int i = 0; i < GLOBAL_SIZE; i++) {
        a[i] = i;
        b[i] = i;
        c[i] = 0;
    }
    int WORKGROUP_SIZE = 64;
    q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
        int index = i.get_local_id();
        c[0] = index;
    }).wait();

    q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
        int index = i.get_group_linear_id();
        c[0] = index;
    }).wait();

    q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
        int index = i.get_global_id();
        c[index] = a[index] + b[index];
    }).wait();

    if (!unitrace_path.empty() && !session.empty()) {
        auto command = unitrace_path + " --resume " + session;
        if (system(command.c_str()) != 0) {
            std::cerr << "[ERROR] Failed to resume session " << session << std::endl;
        }
    }

    WORKGROUP_SIZE = WORKGROUP_SIZE * 2;
    q.parallel_for(nd_range<1>(GLOBAL_SIZE,WORKGROUP_SIZE),[=](auto i) {
        int index = i.get_global_id();
        float f[128];
        
        float x = a[index];
        float y = b[index];

        for (int k = 0; k < 20; k++) {
            for (int i = 0; i < 64; i++) {
                f[i] = x + y + i;
            }

            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 64; j++) {
                    x  = x * f[j];
                    y  = y * f[j];
                }
            }
            c[index] = x + y;
        }
    }).wait();

    if (!unitrace_path.empty() && !session.empty()) {
        auto command = unitrace_path + " --pause " + session;
        if (system(command.c_str()) != 0) {
            std::cerr << "[ERROR] Failed to pause session " << session << std::endl;
		}
    }

    q.wait();

    free((void *)a, q);
    free((void *)b, q);
    free((void *)c, q);

    if (!unitrace_path.empty() && !session.empty()) {
        auto command = unitrace_path + " --stop " + session;
        if (system(command.c_str()) != 0) {
            std::cerr << "[ERROR] Failed to stop session " << session << std::endl;
        }
    }

    return 0;
}

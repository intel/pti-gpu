//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <sycl/sycl.hpp>
#include <iostream>

using namespace sycl;
int main(int argc, char *argv[]) {
    auto preload_env = std::getenv("LD_PRELOAD");
    auto session_env = std::getenv("UNITRACE_Session");
    std::string session;
    if (session_env != nullptr) {
        session = session_env;
    }
    std::string unitrace_path;
    if (preload_env != nullptr) {
        unitrace_path = preload_env;
        auto pos = unitrace_path.find("libunitrace_tool");
        if (pos != std::string::npos) {
            unitrace_path.replace(pos, sizeof("libunitrace_tool.so"), "unitrace"); 
#ifndef _WIN32
            unsetenv("LD_PRELOAD");
#endif /* _WIN32 */
        }
    }

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
        system(command.c_str());
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
        system(command.c_str());
    }

    q.wait();

    free((void *)a, q);
    free((void *)b, q);
    free((void *)c, q);

    if (!unitrace_path.empty() && !session.empty()) {
        auto command = unitrace_path + " --stop " + session;
        system(command.c_str());
    }

    return 0;
}

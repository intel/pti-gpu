//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef DEVICE_MEMORY_H_
#define DEVICE_MEMORY_H_

#include <vector>
#include <CL/sycl.hpp>

// In IPEX/ITEX, device memory are allocated and reused, and released at last.
// here is a very simple mock for this behavior.

struct DeviceMemoryInfo {
    float *data = nullptr;  // float is enough for the demo
    size_t count = 0;
    bool used = 0;
};

class DeviceMemoryManager {
public:
    DeviceMemoryManager() {}
    void init(sycl::queue *q) { this->q = q;}
    void deinit();
    float * alloc(size_t count);
    void free(float *data);
private:
    std::vector<DeviceMemoryInfo> memInfos = {};
    sycl::queue *q = nullptr;
};

inline auto& GlobalDeviceMemoryManager() {
    static DeviceMemoryManager g_devMemMgr = {};
    return g_devMemMgr;
}

//extern DeviceMemoryManager g_devMemMgr;

#endif

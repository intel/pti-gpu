//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <sycl/sycl.hpp>
#include <sycl/ext/oneapi/experimental/graph.hpp>
#include <vector>
#include <iostream>

constexpr size_t N = 256;
constexpr size_t GROUP_SIZE = 64;

namespace syclex = sycl::ext::oneapi::experimental;

void run_kernels(sycl::queue *q, int *d_data, int v) {
    q->parallel_for(sycl::nd_range<1>(N, GROUP_SIZE), [d_data, v](sycl::nd_item<1> i) {
        d_data[i.get_global_id()] = v;
    });
    q->parallel_for(sycl::nd_range<1>(N, GROUP_SIZE), [d_data, v](sycl::nd_item<1> i) {
        d_data[i.get_global_id()] += v;
    });
}

int main() {
    int v = 2;
    std::vector<int> data(N, 0);

    sycl::queue q{sycl::property::queue::in_order{}};
    int *d_data = (int *) sycl::malloc_device(N * sizeof(int), q);
    q.memset(d_data, 0, N * sizeof(int)).wait();

    syclex::command_graph g{q.get_context(), q.get_device()};

    // Record our two kernels
    g.begin_recording(q);
    run_kernels(&q, d_data, v);
    g.end_recording();

    auto eg = g.finalize();
    q.ext_oneapi_graph(eg);
    q.wait_and_throw();

    q.memcpy(data.data(), d_data, sizeof(int) * N).wait();
    sycl::free(d_data, q);

    for (int i = 0; i < N; ++i) {
        if (data[i] != 4) {
            std::cerr << "Test failed at idx: " << i << std::endl;
            return 1;
        }
    }
    std::cerr << "Test passed" << std::endl;
}
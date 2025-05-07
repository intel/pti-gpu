//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>
#include <sycl/sycl.hpp>

void run1(sycl::queue& q, float* dst, float* src, float* tmp1, float* tmp2, int count)
{
    sycl::event ek1 = q.submit([&](sycl::handler& h) {
        h.parallel_for(count, [=](sycl::item<1> item) {
            int idx = item.get_id(0);
            tmp1[idx] = src[idx] * 2;
            });
        });
    sycl::event ek2 = q.submit([&](sycl::handler& h) {
        h.parallel_for(count, [=](sycl::item<1> item) {
            int idx = item.get_id(0);
            tmp2[idx] = tmp1[idx] * 3;
            });
        });
    sycl::event ek3 = q.submit([&](sycl::handler& h) {
        h.parallel_for(count, [=](sycl::item<1> item) {
            int idx = item.get_id(0);
            dst[idx] = tmp2[idx] + 11;
            });
        });
}

int test0()
{
    sycl::queue q{ sycl::gpu_selector_v, {sycl::property::queue::in_order()}};
    int count = 128 * 128;
    float* inp = sycl::malloc_device<float>(count, q);
    float* outp = sycl::malloc_device<float>(count, q);
    float* tmp1 = sycl::malloc_device<float>(count, q);
    float* tmp2 = sycl::malloc_device<float>(count, q);
    float* inp_h = new float[count];
    float* outp_h = new float[count];
    for (size_t i = 0; i < count; ++i) {
        inp_h[i] = i / 4;
        outp_h[i] = -1;
    }

    q.memcpy(inp, inp_h, count * sizeof(float)).wait();

    // record graph
    sycl::ext::oneapi::experimental::command_graph g{ q.get_context(), q.get_device() };
    g.begin_recording(q);
    run1(q, outp, inp, tmp1, tmp2, count);
    g.end_recording();
    auto execGraph = g.finalize();
    q.ext_oneapi_graph(execGraph).wait();
    q.ext_oneapi_graph(execGraph).wait();
    q.ext_oneapi_graph(execGraph).wait();
    q.ext_oneapi_graph(execGraph).wait();
    q.ext_oneapi_graph(execGraph).wait();
    q.ext_oneapi_graph(execGraph).wait();

    q.memcpy(outp_h, outp, count * sizeof(float)).wait();

    std::cout << "test finished." << std::endl;
    return 0;
}

int main(int argc, char** argv)
{
    test0();
}
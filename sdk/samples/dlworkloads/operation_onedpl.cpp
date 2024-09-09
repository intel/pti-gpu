//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "operation_onedpl.h"
#include <oneapi/dpl/cmath>

// onedpl is only used by IPEX as code piece in kernel, while ITEX uses eigen
TinyTensor run_onedpl_operation_cos(const TinyTensor& inp, sycl::queue *q)
{
    TinyTensor outp(inp.N, inp.C, inp.H, inp.W);

    float *src = inp.data;
    float *dst = outp.data;

    q->submit([&](sycl::handler &h) {
        h.parallel_for(outp.count(), [=](sycl::item<1> item) {
            int idx = item.get_id(0);
            dst[idx] = oneapi::dpl::cos(src[idx]);
        });
    });

    return outp;
}

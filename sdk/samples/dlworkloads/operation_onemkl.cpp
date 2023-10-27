//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include <oneapi/mkl/dfti.hpp>
#include "operation_onemkl.h"

// code to just call onemkl function
TinyTensor run_onemkl_operation_fft(const TinyTensor& inp, sycl::queue *q)
{
    TinyTensor outp(inp.N, inp.C, inp.H, inp.W*2);

    int batch = inp.N * inp.C * inp.H;

    std::vector<int64_t> mkl_signal_sizes{inp.W};
    oneapi::mkl::dft::descriptor<oneapi::mkl::dft::precision::SINGLE, oneapi::mkl::dft::domain::REAL> desc(std::move(mkl_signal_sizes));
    desc.set_value(oneapi::mkl::dft::config_param::PLACEMENT, DFTI_NOT_INPLACE);
    desc.set_value(oneapi::mkl::dft::config_param::NUMBER_OF_TRANSFORMS, batch);

    int64_t idist = inp.W;
    int64_t odist = inp.W;
    desc.set_value(oneapi::mkl::dft::config_param::FWD_DISTANCE, idist);
    desc.set_value(oneapi::mkl::dft::config_param::BWD_DISTANCE, odist);

    int64_t signal_ndim = 1;
    std::vector<int64_t> mkl_istrides(1 + signal_ndim, 0);
    std::vector<int64_t> mkl_ostrides(1 + signal_ndim, 0);
    mkl_istrides[1] = 1;
    mkl_ostrides[1] = 1;
    desc.set_value(
        oneapi::mkl::dft::config_param::INPUT_STRIDES, mkl_istrides.data());
    desc.set_value(
        oneapi::mkl::dft::config_param::OUTPUT_STRIDES, mkl_ostrides.data());

    desc.set_value(
        oneapi::mkl::dft::config_param::CONJUGATE_EVEN_STORAGE,
        DFTI_COMPLEX_COMPLEX);

    desc.commit(*q);
    static bool warning_shown = false;
    if (!warning_shown) {
        warning_shown = true;
        // Bollapragada, Datta is working on new API to support allocated external device memory
        std::cout << __FILE__ << ":" << __LINE__;
        std::cout << " internal device memory created in function desc.commit within oneMKL." << std::endl;
    }

    // the conjugate part of the result is not written into outp.data
    oneapi::mkl::dft::compute_forward(desc, inp.data, outp.data);

    return outp;
}

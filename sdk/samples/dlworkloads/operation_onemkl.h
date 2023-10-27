//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef OPERATION_ONEMKL_H_
#define OPERATION_ONEMKL_H_

#include <CL/sycl.hpp>
#include "tiny_tensor.h"

TinyTensor run_onemkl_operation_fft(const TinyTensor& inp, sycl::queue *q);

#endif

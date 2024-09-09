//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef OPERATION_ONEDNN_H_
#define OPERATION_ONEDNN_H_

#include <sycl/sycl.hpp>
#include "tiny_tensor.h"

void onednn_prepare_weights(int oc, int ic, int ks, sycl::queue *q);
TinyTensor run_onednn_operation_conv2d(const TinyTensor& inp, sycl::queue *q);

#endif

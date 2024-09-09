//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef OPERATION_ONEDPL_H_
#define OPERATION_ONEDPL_H_

#include <sycl/sycl.hpp>
#include "tiny_tensor.h"

TinyTensor run_onedpl_operation_cos(const TinyTensor& inp, sycl::queue *q);

#endif

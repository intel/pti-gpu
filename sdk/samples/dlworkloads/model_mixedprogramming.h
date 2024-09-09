//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef MODEL_MIXEDPROGRAMMING_H_
#define MODEL_MIXEDPROGRAMMING_H_

#include <sycl/sycl.hpp>
#include "tiny_tensor.h"

TinyTensor run_model_mixedprogramming(TinyTensor inp, sycl::queue *q);

#endif

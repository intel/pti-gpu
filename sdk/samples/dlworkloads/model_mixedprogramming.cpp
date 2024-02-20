//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "model_mixedprogramming.h"
#include "operation_syclkernel.h"
#include "operation_onednn.h"
#include "operation_onemkl.h"
#include "operation_onedpl.h"

TinyTensor run_model_mixedprogramming(TinyTensor inp, sycl::queue *q)
{
  // the first operation is written with sycl kernel for scale down
  TinyTensor outp = run_syclkernel_operation_scaledown(inp, q);
  GlobalDeviceMemoryManager().free(inp.data);

  inp = outp;
  outp = run_onednn_operation_conv2d(inp, q);
  GlobalDeviceMemoryManager().free(inp.data);

  // next operation uses oneMKL
  inp = outp;
  outp = run_onemkl_operation_fft(inp, q);
  GlobalDeviceMemoryManager().free(inp.data);

  // next operation uses oneDPL
  inp = outp;
  outp = run_onedpl_operation_cos(inp, q);
  GlobalDeviceMemoryManager().free(inp.data);

  return outp;
}

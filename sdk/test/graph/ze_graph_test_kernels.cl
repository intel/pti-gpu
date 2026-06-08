//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "graph_dotproduct_scalars.h"

#define ALPHA PTI_TEST_GRAPH_DOTPRODUCT_ALPHA
#define BETA PTI_TEST_GRAPH_DOTPRODUCT_BETA
#define GAMMA PTI_TEST_GRAPH_DOTPRODUCT_GAMMA

__kernel void InitDotProductVectors(__global float* x, __global float* y, __global float* z) {
  const size_t i = get_global_id(0);
  x[i] = ALPHA;
  y[i] = BETA;
  z[i] = GAMMA;
}

__kernel void VecAddAlpha(__global float* a, __global float* b) {
  const size_t i = get_global_id(0);
  a[i] = ALPHA * a[i] + BETA * b[i];
}

__kernel void VecAddGamma(__global float* a, __global float* b) {
  const size_t i = get_global_id(0);
  a[i] = GAMMA * a[i] + BETA * b[i];
}

__kernel void CalculateDotProduct(__global const float* a, __global const float* b,
                                  __global float* result, const uint vec_size) {
  float sum = 0.0f;
  for (uint j = 0; j < vec_size; j++) {
    sum += a[j] * b[j];
  }
  result[0] = sum;
}

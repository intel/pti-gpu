//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef QUEUE_H_
#define QUEUE_H_

#include <sycl/sycl.hpp>
#include <memory>

std::unique_ptr<sycl::queue> CreateQueue();

#endif

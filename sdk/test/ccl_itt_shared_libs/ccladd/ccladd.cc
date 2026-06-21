//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <ittnotify.h>

static __itt_domain* domain_ccl = nullptr;

void create_domain() {
  if (!domain_ccl) {
    domain_ccl = __itt_domain_create("oneCCL::API");
  }
}

extern "C" {

__attribute__((visibility("default"))) float Add(float x, float y) {
  create_domain();
  static __itt_string_handle* task_add = __itt_string_handle_create("Add");
  __itt_task_begin(domain_ccl, __itt_null, __itt_null, task_add);

  float result = x + y;

  __itt_task_end(domain_ccl);
  return result;
}

__attribute__((visibility("default"))) float Sub(float x, float y) {
  create_domain();
  static __itt_string_handle* task_sub = __itt_string_handle_create("Sub");
  __itt_task_begin(domain_ccl, __itt_null, __itt_null, task_sub);

  float result = x - y;

  __itt_task_end(domain_ccl);
  return result;
}
}

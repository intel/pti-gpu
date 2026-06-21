//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <ittnotify.h>

static __itt_domain* domain_foo1 = nullptr;
static __itt_domain* domain_foo2 = nullptr;

void create_domains() {
  if (!domain_foo1) {
    domain_foo1 = __itt_domain_create("FOO1");
  }
  if (!domain_foo2) {
    domain_foo2 = __itt_domain_create("FOO2");
  }
}
extern "C" {

__attribute__((visibility("default"))) float Mul(float x, float y) {
  create_domains();
  static __itt_string_handle* task_mul = __itt_string_handle_create("Mul");
  __itt_task_begin(domain_foo1, __itt_null, __itt_null, task_mul);

  float result = x * y;

  __itt_task_end(domain_foo1);
  return result;
}

__attribute__((visibility("default"))) float Div(float x, float y) {
  create_domains();
  static __itt_string_handle* task_div = __itt_string_handle_create("Div");
  __itt_task_begin(domain_foo2, __itt_null, __itt_null, task_div);

  float result = x / y;

  __itt_task_end(domain_foo2);
  return result;
}
}

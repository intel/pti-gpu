//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <dlfcn.h>
#include <ittnotify.h>

#include <cstdlib>
#include <iostream>

#include "pti/pti_view.h"

namespace {

bool ExpectCoreUnloaded(const char* phase) {
  // RTLD_NOLOAD observes the loader state without mapping the core.
  void* core = dlopen(PTI_TEST_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_NOLOAD);
  if (core == nullptr) {
    return true;
  }
  std::cerr << "PTI core is already loaded " << phase << '\n';
  if (dlclose(core) != 0) {
    std::cerr << "Failed to close the core observation handle\n";
  }
  return false;
}

}  // namespace

int main() {
  if (!ExpectCoreUnloaded("before ITT setup")) {
    return EXIT_FAILURE;
  }

  auto domain = __itt_domain_create("oneCCL::API");
  auto task = __itt_string_handle_create("BeforeEnable");
  if (domain == nullptr || task == nullptr) {
    std::cerr << "Failed to create the ITT domain or task handle\n";
    return EXIT_FAILURE;
  }
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_task_end(domain);
  if (!ExpectCoreUnloaded("after ITT calls")) {
    return EXIT_FAILURE;
  }

  // The enable request loads the core before reporting missing callbacks.
  const auto result = ptiViewEnable(PTI_VIEW_COMMUNICATION);
  if (result != PTI_ERROR_NO_CALLBACKS_SET) {
    std::cerr << "Expected PTI_ERROR_NO_CALLBACKS_SET, received " << result << '\n';
    return EXIT_FAILURE;
  }
  void* core = dlopen(PTI_TEST_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_NOLOAD);
  if (core == nullptr) {
    std::cerr << "PTI core is not loaded after the enable request\n";
    return EXIT_FAILURE;
  }
  if (dlclose(core) != 0) {
    std::cerr << "Failed to close the core observation handle\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

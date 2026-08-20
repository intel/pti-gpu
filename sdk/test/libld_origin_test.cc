//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <dlfcn.h>
#include <gtest/gtest.h>

#include <iostream>
#include <memory>

namespace {

const char* LibraryPathForSymbol(const void* symbol) {
  Dl_info info{};
  if (dladdr(symbol, &info) == 0 || info.dli_fname == nullptr) {
    return "<unknown>";
  }
  return info.dli_fname;
}

}  // namespace

// This synthetic Core-like library has RUNPATH $ORIGIN and a colocated dependency.
// Loading it directly proves normal ELF $ORIGIN resolution without PTI's preloader.
TEST(OriginResolution, CoreResolvesColocatedDependency) {
  dlerror();
  void* raw_handle = dlopen(PTI_ORIGIN_CORE_LIBRARY_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(raw_handle, nullptr) << "Failed to dlopen synthetic Core library: " << dlerror();
  auto handle = std::unique_ptr<void, decltype(&dlclose)>(raw_handle, dlclose);

  using CoreSymbol = int (*)();
  dlerror();
  auto core_symbol =
      reinterpret_cast<CoreSymbol>(dlsym(handle.get(), "PtiPreloadStub_core_origin"));
  ASSERT_NE(core_symbol, nullptr) << "Failed to find synthetic Core symbol: " << dlerror();

  dlerror();
  auto dependency_symbol =
      reinterpret_cast<CoreSymbol>(dlsym(handle.get(), "PtiPreloadStub_dep_origin"));
  ASSERT_NE(dependency_symbol, nullptr)
      << "Failed to find colocated dependency symbol: " << dlerror();

  std::cout << "Requested Core library: " << PTI_ORIGIN_CORE_LIBRARY_PATH << '\n'
            << "Core symbol resolved from: "
            << LibraryPathForSymbol(reinterpret_cast<const void*>(core_symbol)) << '\n'
            << "Dependency symbol resolved from: "
            << LibraryPathForSymbol(reinterpret_cast<const void*>(dependency_symbol)) << std::endl;

  EXPECT_EQ(core_symbol(), 0);
  EXPECT_EQ(dependency_symbol(), 0);
}

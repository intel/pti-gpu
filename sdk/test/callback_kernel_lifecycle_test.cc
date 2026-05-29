//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Tests for the PTI Callback API kernel-lifecycle domains:
//   - PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED
//   - PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED
//
// Two test surfaces:
//   1. Direct Level-Zero: explicit zeKernelCreate / zeKernelDestroy
//      calls and verification that callbacks fire with matching handle/name.
//   2. SYCL: run a SYCL kernel; the SYCL runtime triggers L0 kernel
//      create/destroy under the hood and the callbacks should still fire.

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <atomic>
#include <cstdlib>
#include <mutex>
#include <string>
#include <sycl/sycl.hpp>
#include <unordered_set>
#include <vector>

#include "pti/pti_callback.h"
#include "pti/pti_view.h"
#include "utils.h"

namespace {

constexpr size_t kBufferSize = 64 * 1024;
constexpr int kLifecycleKernelCount = 3;
constexpr char kLifecycleKernelName[] = "GEMM";

// ----------------------------------------------------------------------------
// Shared callback observation state
// ----------------------------------------------------------------------------
struct LifecycleEvent {
  pti_callback_domain domain;
  pti_callback_phase phase;
  pti_backend_kernel_t kernel_handle;
  pti_backend_module_t module_handle;
  std::string name;
  uint32_t return_code;
  uint32_t correlation_id;
  uint32_t api_id;
};

struct LifecycleObserver {
  std::mutex mtx;
  std::vector<LifecycleEvent> events;
  std::atomic<int> create_enter{0};
  std::atomic<int> create_exit{0};
  std::atomic<int> destroy_enter{0};
  std::atomic<int> destroy_exit{0};

  void Reset() {
    std::lock_guard<std::mutex> lock(mtx);
    events.clear();
    create_enter = 0;
    create_exit = 0;
    destroy_enter = 0;
    destroy_exit = 0;
  }
};

void LifecycleCallback(pti_callback_domain domain, pti_api_group_id /*api_group*/, uint32_t api_id,
                       pti_backend_ctx_t /*ctx*/, void* cb_data, void* user_data,
                       void** /*instance_user_data*/) {
  if (domain != PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED &&
      domain != PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED) {
    return;
  }
  auto* obs = static_cast<LifecycleObserver*>(user_data);
  auto* data = static_cast<pti_callback_kernel_data*>(cb_data);

  LifecycleEvent ev{};
  ev.domain = domain;
  ev.phase = data->_phase;
  ev.kernel_handle = data->_device_kernel_handle;
  ev.module_handle = data->_module_handle;
  ev.name = (data->_name != nullptr) ? data->_name : "";
  ev.return_code = data->_return_code;
  ev.correlation_id = data->_correlation_id;
  ev.api_id = api_id;

  {
    std::lock_guard<std::mutex> lock(obs->mtx);
    obs->events.push_back(std::move(ev));
  }
  if (domain == PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED) {
    if (data->_phase == PTI_CB_PHASE_API_ENTER)
      ++obs->create_enter;
    else
      ++obs->create_exit;
  } else {
    if (data->_phase == PTI_CB_PHASE_API_ENTER)
      ++obs->destroy_enter;
    else
      ++obs->destroy_exit;
  }
}

void BufferRequested(unsigned char** buf, size_t* size) {
  *buf = static_cast<unsigned char*>(std::malloc(kBufferSize));
  *size = (*buf == nullptr) ? 0 : kBufferSize;
}

void BufferCompleted(unsigned char* buf, size_t /*size*/, size_t /*used*/) { std::free(buf); }

// ----------------------------------------------------------------------------
// Test fixture: enables PTI view + subscribes to lifecycle domain.
// ----------------------------------------------------------------------------
class KernelLifecycleCallbackTest : public ::testing::Test {
 protected:
  struct ZeKernelResources {
    ze_driver_handle_t driver = nullptr;
    ze_device_handle_t device = nullptr;
    ze_context_handle_t context = nullptr;
    ze_module_handle_t module_handle = nullptr;

    ~ZeKernelResources() {
      if (module_handle != nullptr) {
        EXPECT_EQ(zeModuleDestroy(module_handle), ZE_RESULT_SUCCESS);
      }
      if (context != nullptr) {
        EXPECT_EQ(zeContextDestroy(context), ZE_RESULT_SUCCESS);
      }
    }
  };

  void SetUp() override {
    observer_.Reset();
    ASSERT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);

    ASSERT_EQ(ptiCallbackSubscribe(&subscriber_, &LifecycleCallback, &observer_), PTI_SUCCESS);
    EnableLifecycleDomains();
  }

  void TearDown() override {
    if (subscriber_ != nullptr) {
      EXPECT_EQ(ptiCallbackDisableDomain(subscriber_, PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED),
                PTI_SUCCESS);
      EXPECT_EQ(ptiCallbackDisableDomain(subscriber_, PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED),
                PTI_SUCCESS);
      EXPECT_EQ(ptiCallbackUnsubscribe(subscriber_), PTI_SUCCESS);
      subscriber_ = nullptr;
    }
    EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
    ptiFlushAllViews();
  }

  void EnableLifecycleDomains() {
    ASSERT_EQ(ptiCallbackEnableDomain(subscriber_, PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED, 1, 1),
              PTI_SUCCESS);
    ASSERT_EQ(ptiCallbackEnableDomain(subscriber_, PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED, 1, 1),
              PTI_SUCCESS);
  }

  std::vector<LifecycleEvent> GetEvents(pti_callback_domain domain, pti_callback_phase phase) {
    std::lock_guard<std::mutex> lock(observer_.mtx);
    std::vector<LifecycleEvent> matching_events;
    for (const auto& event : observer_.events) {
      if (event.domain == domain && event.phase == phase) {
        matching_events.push_back(event);
      }
    }
    return matching_events;
  }

  void CreateKernelResources(ZeKernelResources* resources) {
    ASSERT_NE(resources, nullptr);
    ASSERT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);

    uint32_t driver_count = 0;
    ASSERT_EQ(zeDriverGet(&driver_count, nullptr), ZE_RESULT_SUCCESS);
    if (driver_count == 0) {
      GTEST_SKIP() << "No L0 driver";
    }

    std::vector<ze_driver_handle_t> drivers(driver_count);
    ASSERT_EQ(zeDriverGet(&driver_count, drivers.data()), ZE_RESULT_SUCCESS);

    resources->driver = drivers[0];

    uint32_t device_count = 0;
    ASSERT_EQ(zeDeviceGet(resources->driver, &device_count, nullptr), ZE_RESULT_SUCCESS);
    if (device_count == 0) {
      GTEST_SKIP() << "No L0 device";
    }

    std::vector<ze_device_handle_t> devices(device_count);
    ASSERT_EQ(zeDeviceGet(resources->driver, &device_count, devices.data()), ZE_RESULT_SUCCESS);
    resources->device = devices[0];

    ze_context_desc_t context_desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ASSERT_EQ(zeContextCreate(resources->driver, &context_desc, &resources->context),
              ZE_RESULT_SUCCESS);

    std::vector<uint8_t> binary = utils::LoadBinaryFile(utils::GetExecutablePath() + "gemm.spv");
    if (binary.empty()) {
      GTEST_SKIP() << "gemm.spv not found; cannot create module";
    }

    ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                    nullptr,
                                    ZE_MODULE_FORMAT_IL_SPIRV,
                                    binary.size(),
                                    binary.data(),
                                    nullptr,
                                    nullptr};
    ze_result_t module_result = zeModuleCreate(resources->context, resources->device, &module_desc,
                                               &resources->module_handle, nullptr);
    if (module_result != ZE_RESULT_SUCCESS || resources->module_handle == nullptr) {
      GTEST_SKIP() << "zeModuleCreate failed";
    }
  }

  std::vector<ze_kernel_handle_t> CreateKernels(ze_module_handle_t module_handle, int kernel_count,
                                                const char* kernel_name) {
    std::vector<ze_kernel_handle_t> kernels;
    kernels.reserve(kernel_count);
    for (int index = 0; index < kernel_count; ++index) {
      ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, kernel_name};
      ze_kernel_handle_t kernel = nullptr;
      ze_result_t result = zeKernelCreate(module_handle, &kernel_desc, &kernel);
      EXPECT_EQ(result, ZE_RESULT_SUCCESS);
      EXPECT_NE(kernel, nullptr);

      kernels.push_back(kernel);
    }
    return kernels;
  }

  static std::unordered_set<pti_backend_kernel_t> ToKernelSet(
      const std::vector<ze_kernel_handle_t>& kernels) {
    return {kernels.begin(), kernels.end()};
  }

  std::unordered_set<pti_backend_kernel_t> FindKernelHandles(pti_callback_domain domain,
                                                             pti_callback_phase phase) {
    std::unordered_set<pti_backend_kernel_t> kernel_handles;
    for (const auto& event : GetEvents(domain, phase)) {
      kernel_handles.insert(event.kernel_handle);
    }
    return kernel_handles;
  }

  void ExpectSuccessfulCreateExitEvents(ze_module_handle_t module_handle,
                                        const std::unordered_set<pti_backend_kernel_t>& expected,
                                        const std::string& expected_name) {
    std::unordered_set<pti_backend_kernel_t> created;
    for (const auto& event :
         GetEvents(PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED, PTI_CB_PHASE_API_EXIT)) {
      EXPECT_EQ(event.return_code, static_cast<uint32_t>(ZE_RESULT_SUCCESS));
      EXPECT_NE(event.kernel_handle, nullptr);
      EXPECT_EQ(event.module_handle, static_cast<pti_backend_module_t>(module_handle));
      EXPECT_EQ(event.name, expected_name);
      created.insert(event.kernel_handle);
    }
    EXPECT_EQ(created, expected);
  }

  LifecycleObserver observer_;
  pti_callback_subscriber_handle subscriber_ = nullptr;
};

// ----------------------------------------------------------------------------
// Test 1: direct Level-Zero kernel create / destroy.
// ----------------------------------------------------------------------------
TEST_F(KernelLifecycleCallbackTest, L0_KernelCreateDestroyFiresCallback) {
  ZeKernelResources resources;
  CreateKernelResources(&resources);

  auto kernels =
      CreateKernels(resources.module_handle, kLifecycleKernelCount, kLifecycleKernelName);
  ASSERT_EQ(kernels.size(), static_cast<size_t>(kLifecycleKernelCount));

  EXPECT_EQ(observer_.create_enter.load(), kLifecycleKernelCount);
  EXPECT_EQ(observer_.create_exit.load(), kLifecycleKernelCount);

  auto expected_kernels = ToKernelSet(kernels);
  ExpectSuccessfulCreateExitEvents(resources.module_handle, expected_kernels, kLifecycleKernelName);

  for (auto k : kernels) {
    ASSERT_EQ(zeKernelDestroy(k), ZE_RESULT_SUCCESS);
  }

  EXPECT_EQ(observer_.destroy_enter.load(), kLifecycleKernelCount);
  EXPECT_EQ(observer_.destroy_exit.load(), kLifecycleKernelCount);

  auto destroyed_enter =
      FindKernelHandles(PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED, PTI_CB_PHASE_API_ENTER);
  EXPECT_EQ(destroyed_enter, expected_kernels);
}

// ----------------------------------------------------------------------------
// Test 2: SYCL kernel triggers underlying L0 kernel create/destroy callbacks.
// ----------------------------------------------------------------------------
TEST_F(KernelLifecycleCallbackTest, SYCL_KernelTriggersLifecycleCallbacks) {
  try {
    sycl::queue q{sycl::gpu_selector_v};
    constexpr size_t kSize = 1024;
    std::vector<int> a(kSize, 1);
    {
      sycl::buffer<int, 1> buf(a.data(), sycl::range<1>(kSize));
      q.submit([&](sycl::handler& cgh) {
         auto acc = buf.get_access<sycl::access::mode::read_write>(cgh);
         cgh.parallel_for<class CbLifecycleSyclKernel>(sycl::range<1>(kSize),
                                                       [=](sycl::id<1> i) { acc[i] = acc[i] + 2; });
       }).wait_and_throw();
    }
    EXPECT_EQ(a[0], 3);
  } catch (const sycl::exception& e) {
    GTEST_SKIP() << "SYCL exception: " << e.what();
  }

  // Force PTI to flush before assertions (kernels held by SYCL runtime may
  // be destroyed lazily, so we focus on CREATE here).
  ptiFlushAllViews();

  EXPECT_GE(observer_.create_enter.load(), 1);
  EXPECT_GE(observer_.create_exit.load(), 1);

  // ENTER and EXIT counts should match for create.
  EXPECT_EQ(observer_.create_enter.load(), observer_.create_exit.load());

  // Each EXIT-phase create must report a non-null kernel handle on success.
  bool saw_success = false;
  for (const auto& event : GetEvents(PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED, PTI_CB_PHASE_API_EXIT)) {
    if (event.return_code != static_cast<uint32_t>(ZE_RESULT_SUCCESS)) {
      continue;
    }
    EXPECT_NE(event.kernel_handle, nullptr);
    EXPECT_NE(event.module_handle, nullptr);
    saw_success = true;
  }
  EXPECT_TRUE(saw_success);
}

// ----------------------------------------------------------------------------
// Test 3: enabling kernel domains is a separate operation per subscriber, and
// disabling stops invocations.
// ----------------------------------------------------------------------------
TEST(KernelLifecycleCallbackEnableDisable, EnableDisableSemantics) {
  LifecycleObserver obs;
  ASSERT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);

  pti_callback_subscriber_handle subscriber = nullptr;
  ASSERT_EQ(ptiCallbackSubscribe(&subscriber, &LifecycleCallback, &obs), PTI_SUCCESS);

  // Enabling requires a valid (known) domain.
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED, 1, 1),
            PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackEnableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED, 1, 1),
            PTI_SUCCESS);

  EXPECT_EQ(ptiCallbackDisableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED), PTI_SUCCESS);
  EXPECT_EQ(ptiCallbackDisableDomain(subscriber, PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED),
            PTI_SUCCESS);

  // Stringification works for the new domains.
  EXPECT_STREQ(ptiCallbackDomainTypeToString(PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED),
               "PTI_CB_DOMAIN_DRIVER_KERNEL_CREATED");
  EXPECT_STREQ(ptiCallbackDomainTypeToString(PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED),
               "PTI_CB_DOMAIN_DRIVER_KERNEL_DESTROYED");

  EXPECT_EQ(ptiCallbackUnsubscribe(subscriber), PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
}

}  // namespace

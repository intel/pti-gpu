//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#include "queue.h"

std::unique_ptr<sycl::queue> CreateQueue()
{
  auto plaform_list = sycl::platform::get_platforms();
  std::vector<sycl::device> root_devices;
  // Enumerated root devices(GPU cards) from GPU Platform firstly.
  for (const auto& platform : plaform_list) {
    if (platform.get_backend() != sycl::backend::ext_oneapi_level_zero)
      continue;
    auto device_list = platform.get_devices();
    for (const auto& device : device_list) {
      if (device.is_gpu()) {
        root_devices.push_back(device);
      }
    }
  }

  std::cout << root_devices.size() << " GPU root devices (cards) are found." << std::endl;
  std::cout << "//export EnableImplicitScaling=1 to show the full device memory capability of the root device." << std::endl;
  if (root_devices.size() == 0) {
    return NULL;
  }
  for (std::size_t i = 0; i < root_devices.size(); ++i) {
    const auto& root_device = root_devices[i];
    std::cout << "  " << i << ") ";
    std::cout << "root device: " << root_device.get_info<sycl::info::device::name>();
    std::cout << " (" << root_device.get_info<sycl::info::device::global_mem_size>()/1024./1024./1024. << "GiB)";
    std::cout << " in platform: " << root_device.get_platform().get_info<sycl::info::platform::name>() << std::endl;
  }
  std::cout << std::endl;

  auto& root_device = root_devices[0];
  sycl::device dev = root_device;
  try {
    // the default behavior of IPEX is to consider one tile as a card
    constexpr sycl::info::partition_property partition_by_affinity = sycl::info::partition_property::partition_by_affinity_domain;
    constexpr sycl::info::partition_affinity_domain next_partitionable = sycl::info::partition_affinity_domain::next_partitionable;
    std::vector<sycl::device> sub_devices = root_device.create_sub_devices<partition_by_affinity>(next_partitionable);
    dev = sub_devices[0];
    std::cout << sub_devices.size() << " sub devices found in the first root device, try the first sub device: " << dev.get_info<sycl::info::device::name>();
    std::cout << " (" << dev.get_info<sycl::info::device::global_mem_size>()/1024./1024./1024. << "GiB)" << std::endl;
  } catch (...) {
    std::cout << "no sub device found in the first root device, continue to use the first root device." << std::endl;
  }

  std::cout << "Driver: "
            << dev.get_info<sycl::info::device::driver_version>()
            << std::endl;

  // both IPEX and ITEX use in order sycl queue
  auto q = std::make_unique<sycl::queue>(sycl::queue{dev, {sycl::property::queue::in_order(),
                                          sycl::property::queue::enable_profiling()}});

  return q;
}

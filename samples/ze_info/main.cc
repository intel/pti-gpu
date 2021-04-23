#include <vector>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "pti_assert.h"

#define TAB "  "
#define TEXT_WIDTH  50
#define BYTES_IN_KB 1024
#define BYTES_IN_MB (1024 * 1024)
#define BYTES_IN_GB (1024 * 1024 * 1024)

std::string ConvertBytesToString(size_t value) {
  if (value / BYTES_IN_KB < 1) {
    return std::to_string(value) + "B";
  }
  if (value / BYTES_IN_MB < 1) {
    if (value % BYTES_IN_KB != 0) {
      std::stringstream tempstream;
      tempstream << std::fixed << std::setprecision(2) <<
        static_cast<float>(value) / BYTES_IN_KB << "KiB";
      return tempstream.str();
    }
    return std::to_string(value / BYTES_IN_KB) + "KiB";
  }
  if (value / BYTES_IN_GB < 1) {
    if (value % BYTES_IN_MB != 0) {
      std::stringstream tempstream;
      tempstream << std::fixed << std::setprecision(2) <<
        static_cast<float>(value) / BYTES_IN_MB << "MiB";
      return tempstream.str();
    }
    return std::to_string(value / BYTES_IN_MB) + "MiB";
  }
  if (value % BYTES_IN_GB != 0) {
    std::stringstream tempstream;
    tempstream << std::fixed << std::setprecision(2) <<
      static_cast<float>(value) / BYTES_IN_GB << "GiB";
    return tempstream.str();
  }
  return std::to_string(value / BYTES_IN_GB) + "GiB";
}

int main(int argc, char *argv[]) {
  bool list_mode = false;

  for (size_t i = 1; i < argc; i++) {
    if (std::string(argv[i]).compare("-l") == 0) {
      list_mode = true;
    }
  }

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  // Discover all the driver instances
  uint32_t driver_count = 0;
  status = zeDriverGet(&driver_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || driver_count == 0) {
    return 0;
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count, nullptr);
  status = zeDriverGet(&driver_count, driver_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (list_mode) {
    for (uint32_t i = 0; i < driver_count; ++i) {
      ze_api_version_t version;
      status = zeDriverGetApiVersion(driver_list[i], &version);
      if (status != ZE_RESULT_SUCCESS) {
        continue;
      }
      std::cout << "Driver #" << i << ": API Version "
                << ZE_MAJOR_VERSION(version) << "." << ZE_MINOR_VERSION(version);
      if (version == ZE_API_VERSION_CURRENT) {
        std::cout << " (latest)";
      }
      std::cout << std::endl;

      uint32_t device_count = 0;
      status = zeDeviceGet(driver_list[i], &device_count, nullptr);
      if (status != ZE_RESULT_SUCCESS || device_count == 0) {
        continue;
      }

      std::vector<ze_device_handle_t> device_list(device_count, nullptr);
      status = zeDeviceGet(driver_list[i], &device_count, device_list.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      for (uint32_t j = 0; j < device_count; ++j) {
        ze_device_properties_t device_properties;
        status = zeDeviceGetProperties(device_list[j], &device_properties);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::cout << "-- Device #" << j << ": " <<
          device_properties.name << std::endl;

        uint32_t sub_device_count = 0;
        status = zeDeviceGetSubDevices(
            device_list[j], &sub_device_count, nullptr);
        if (status != ZE_RESULT_SUCCESS || sub_device_count == 0) {
          continue;
        }

        std::vector<ze_device_handle_t> sub_device_list(sub_device_count, nullptr);
        status = zeDeviceGetSubDevices(
            device_list[j], &sub_device_count, sub_device_list.data());
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        for (uint32_t k = 0; k < sub_device_count; ++k) {
          ze_device_properties_t sub_device_properties;
          status = zeDeviceGetProperties(sub_device_list[k], &sub_device_properties);
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          std::cout << "---- Subdevice #" << k << ": " <<
            sub_device_properties.name << std::endl;
        }
      }
    }
  } else {
    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      "Number of drivers" << driver_count << std::endl;

    for (uint32_t i = 0; i < driver_count; ++i) {
      ze_api_version_t version;
      status = zeDriverGetApiVersion(driver_list[i], &version);
      if (status != ZE_RESULT_SUCCESS) {
        continue;
      }
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Driver API Version " <<
          ZE_MAJOR_VERSION(version) << "." << ZE_MINOR_VERSION(version);
      if (version == ZE_API_VERSION_CURRENT) {
        std::cout << " (latest)";
      }
      std::cout << std::endl;

      ze_driver_properties_t driver_properties;
      status = zeDriverGetProperties(driver_list[i], &driver_properties);
      if (status != ZE_RESULT_SUCCESS) {
        continue;
      }

      std::cout << std::setw(TEXT_WIDTH) <<
        std::left << std::string() + TAB + "Driver Version " <<
        driver_properties.driverVersion << std::endl << std::endl;
    }

    for (uint32_t i = 0; i < driver_count; ++i) {
      uint32_t device_count = 0;
      status = zeDeviceGet(driver_list[i], &device_count, nullptr);
      if (status != ZE_RESULT_SUCCESS || device_count == 0) {
          continue;
      }

      std::vector<ze_device_handle_t> device_list(device_count, nullptr);
      status = zeDeviceGet(driver_list[i], &device_count, device_list.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Driver " << i << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        "Number of devices " << device_count << std::endl;

      for (uint32_t j = 0; j < device_count; ++j) {
        ze_device_properties_t device_properties;
        status = zeDeviceGetProperties(device_list[j], &device_properties);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Device Name " <<
          device_properties.name << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Device Type ";

        switch(device_properties.type) {
          case ZE_DEVICE_TYPE_GPU:
            std::cout << "GPU";
            break;
          case ZE_DEVICE_TYPE_CPU:
            std::cout << "CPU";
            break;
          case ZE_DEVICE_TYPE_FPGA:
            std::cout << "FPGA";
            break;
          case ZE_DEVICE_TYPE_MCA:
            std::cout << "MCA";
            break;
          default:
            std::cout << "OTHER";
            break;
        }
        std::cout << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Vendor ID " << "0x" << std::hex <<
          device_properties.vendorId << std::dec << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Device ID " << "0x" << std::hex <<
          device_properties.deviceId << std::dec << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Subdevice ID " << "0x" << std::hex <<
          device_properties.subdeviceId << std::dec << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Core Clock Rate " <<
          device_properties.coreClockRate << "MHz" << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Maximum Memory Allocation Size " <<
          device_properties.maxMemAllocSize << " (" <<
          ConvertBytesToString(device_properties.maxMemAllocSize) <<
          ")" << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Maximum Hardware Contexts " <<
          device_properties.maxHardwareContexts << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Maximum Command Queue Priority " <<
          device_properties.maxCommandQueuePriority << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Number Threads Per EU " <<
          device_properties.numThreadsPerEU << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Physical EU SIMD Width " <<
          device_properties.physicalEUSimdWidth << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Number EU Per SubSlice " <<
          device_properties.numEUsPerSubslice << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Number SubSlices Per Slice " <<
          device_properties.numSubslicesPerSlice << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Number Slices " <<
          device_properties.numSlices << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + "Timer Resolution " <<
          device_properties.timerResolution << "ns" << std::endl;
      }
    }
  }

  return 0;
}

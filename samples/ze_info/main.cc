#include <vector>
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <utility>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include "pti_assert.h"
#include "utils.h"
#include "ze_utils.h"

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

void PrintDeviceList() {
  ze_result_t status = ZE_RESULT_SUCCESS;

  std::vector<ze_driver_handle_t> driver_list = utils::ze::GetDriverList();
  if (driver_list.empty()) {
    return;
  }

  for (size_t i = 0; i < driver_list.size(); ++i) {
    ze_api_version_t version = utils::ze::GetDriverVersion(driver_list[i]);
    PTI_ASSERT(version != ZE_API_VERSION_FORCE_UINT32);

    std::cout << "Driver #" << i << ": API Version " <<
      ZE_MAJOR_VERSION(version) << "." << ZE_MINOR_VERSION(version);
    if (version == ZE_API_VERSION_CURRENT) {
      std::cout << " (latest)";
    }
    std::cout << std::endl;

    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList(driver_list[i]);
    if (device_list.empty()) {
      continue;
    }

    for (size_t j = 0; j < device_list.size(); ++j) {
      ze_device_properties_t device_props{
          ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      status = zeDeviceGetProperties(device_list[j], &device_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      std::cout << "-- Device #" << j << ": " <<
        device_props.name << std::endl;


      std::vector<ze_device_handle_t> sub_device_list =
        utils::ze::GetSubDeviceList(device_list[j]);
      if (sub_device_list.empty()) {
        continue;
      }

      for (size_t k = 0; k < sub_device_list.size(); ++k) {
        ze_device_properties_t sub_device_props{
            ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
        status = zeDeviceGetProperties(
            sub_device_list[k], &sub_device_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::cout << "---- Subdevice #" << k << ": " <<
          sub_device_props.name << std::endl;
      }
    }
  }
}

void PrintFloatingPointFlags(std::string tabs, ze_device_fp_flags_t value) {
  static const std::pair<ze_device_fp_flag_t, const char *> flags[] = {
    {ZE_DEVICE_FP_FLAG_DENORM, "Denormals "},
    {ZE_DEVICE_FP_FLAG_INF_NAN, "Infinity and NaN "},
    {ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST, "Round to nearest even "},
    {ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO, "Round to zero "},
    {ZE_DEVICE_FP_FLAG_ROUND_TO_INF, "Round to infinity "},
    {ZE_DEVICE_FP_FLAG_FMA, "IEEE754-2008 fused multiply-add "},
    {ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT, "Correctly-rounded Div Sqrt "},
    {ZE_DEVICE_FP_FLAG_SOFT_FLOAT, "Support is emulated in software "},
  };

  for (auto &v: flags) {
    ze_device_fp_flag_t flag = v.first;
    const char* message = v.second;

    std::cout << std::setw(TEXT_WIDTH) << std::left << tabs + message <<
      (value & flag ? "yes" : "no") << std::endl;
  }
}

void PrintDeviceInfo() {
  ze_result_t status = ZE_RESULT_SUCCESS;

  std::vector<ze_driver_handle_t> driver_list = utils::ze::GetDriverList();
  if (driver_list.empty()) {
    return;
  }

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Number of drivers" << driver_list.size() << std::endl;

  for (size_t i = 0; i < driver_list.size(); ++i) {
    ze_api_version_t version = utils::ze::GetDriverVersion(driver_list[i]);
    PTI_ASSERT(version != ZE_API_VERSION_FORCE_UINT32);

    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      std::string() + TAB + "Driver API Version " <<
      ZE_MAJOR_VERSION(version) << "." << ZE_MINOR_VERSION(version);
    if (version == ZE_API_VERSION_CURRENT) {
      std::cout << " (latest)";
    }
    std::cout << std::endl;

    ze_driver_properties_t driver_props{
        ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES, };
    status = zeDriverGetProperties(driver_list[i], &driver_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::cout << std::setw(TEXT_WIDTH) <<
      std::left << std::string() + TAB + "Driver Version " <<
      driver_props.driverVersion << std::endl << std::endl;
  }

  for (size_t i = 0; i < driver_list.size(); ++i) {
    std::vector<ze_device_handle_t> device_list =
      utils::ze::GetDeviceList(driver_list[i]);
    if (device_list.empty()) {
      continue;
    }

    ze_api_version_t version = utils::ze::GetDriverVersion(driver_list[i]);
    PTI_ASSERT(version != ZE_API_VERSION_FORCE_UINT32);

    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      std::string() + TAB + "Driver " << i << std::endl;

    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      "Number of devices " << device_list.size() << std::endl;

    for (size_t j = 0; j < device_list.size(); ++j) {
      ze_device_properties_t device_props{
          ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      status = zeDeviceGetProperties(device_list[j], &device_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      ze_device_compute_properties_t compute_props{
        ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES, };
      status = zeDeviceGetComputeProperties(device_list[j], &compute_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      ze_device_module_properties_t module_props{
        ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES, };
      status = zeDeviceGetModuleProperties(device_list[j], &module_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Device Name " <<
        device_props.name << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Device Type ";

      switch(device_props.type) {
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
        device_props.vendorId << std::dec << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Device ID " << "0x" << std::hex <<
        device_props.deviceId << std::dec << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Subdevice ID " << "0x" << std::hex <<
        device_props.subdeviceId << std::dec << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Core Clock Rate " <<
        device_props.coreClockRate << "MHz" << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Maximum Memory Allocation Size " <<
        device_props.maxMemAllocSize << " (" <<
        ConvertBytesToString(device_props.maxMemAllocSize) <<
        ")" << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Maximum Hardware Contexts " <<
        device_props.maxHardwareContexts << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Maximum Command Queue Priority " <<
        device_props.maxCommandQueuePriority << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Number Threads Per EU " <<
        device_props.numThreadsPerEU << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Physical EU SIMD Width " <<
        device_props.physicalEUSimdWidth << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Number EU Per SubSlice " <<
        device_props.numEUsPerSubslice << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Number SubSlices Per Slice " <<
        device_props.numSubslicesPerSlice << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Number Slices " <<
        device_props.numSlices << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Timer Resolution " <<
        device_props.timerResolution;
      if (version >= ZE_API_VERSION_1_0) {
        std::cout << "ns";
      } else {
        std::cout << "clks";
      }
      std::cout << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Compute properties: " << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Maximum workgroup size " <<
        compute_props.maxTotalGroupSize << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Maximum workgroup sizes (X, Y, Z) " <<
        compute_props.maxGroupSizeX << 'x' <<
        compute_props.maxGroupSizeY << 'x' <<
        compute_props.maxGroupSizeZ << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Maximum workgroup count (X, Y, Z) " <<
        compute_props.maxGroupCountX << 'x' <<
        compute_props.maxGroupCountY << 'x' <<
        compute_props.maxGroupCountZ << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB +
        "Maximum Shared Local Memory Size Per Group" <<
        compute_props.maxSharedLocalMemory << " (" <<
        ConvertBytesToString(compute_props.maxSharedLocalMemory) <<
        ")" << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Subgroup Sizes Supported ";

      for (uint32_t i = 0; i < compute_props.numSubGroupSizes; i++) {
        std::cout << compute_props.subGroupSizes[i] << ' ';
      }
      std::cout << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + "Module properties: " << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "SPIR-V supported version " <<
        ZE_MAJOR_VERSION(module_props.spirvVersionSupported) << '.' <<
        ZE_MINOR_VERSION(module_props.spirvVersionSupported) << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Flags ";
      if (module_props.flags) {
        std::cout <<
          (module_props.flags & ZE_DEVICE_MODULE_FLAG_FP16 ? "fp16 " : "") <<
          (module_props.flags & ZE_DEVICE_MODULE_FLAG_FP64 ? "fp64 " : "") <<
          (module_props.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS ?
            "int64_atomics " : "") <<
          (module_props.flags & ZE_DEVICE_MODULE_FLAG_DP4A ? "dp4a " : "");
      } else {
        std::cout << "(none)";
      }
      std::cout << std::endl;

      if (module_props.flags & ZE_DEVICE_MODULE_FLAG_FP16) {
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + TAB + "fp16 properties:" << std::endl;
        PrintFloatingPointFlags(
            std::string() + TAB + TAB + TAB, module_props.fp16flags);
      }

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "fp32 properties:" << std::endl;
      PrintFloatingPointFlags(
          std::string() + TAB + TAB + TAB, module_props.fp32flags);

      if (module_props.flags & ZE_DEVICE_MODULE_FLAG_FP64) {
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          std::string() + TAB + TAB + "fp64 properties:" << std::endl;
        PrintFloatingPointFlags(
          std::string() + TAB + TAB + TAB, module_props.fp64flags);
      }

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Maximum kernel arguments size " <<
        module_props.maxArgumentsSize << " (" <<
        ConvertBytesToString(module_props.maxArgumentsSize) <<
        ")" << std::endl;

      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        std::string() + TAB + TAB + "Print buffer size " <<
        module_props.printfBufferSize << " (" <<
        ConvertBytesToString(module_props.printfBufferSize) <<
        ")" << std::endl;
    }
  }
}

int main(int argc, char *argv[]) {
  bool list_mode = false;

  for (size_t i = 1; i < argc; i++) {
    if (std::string(argv[i]).compare("-l") == 0) {
      list_mode = true;
    }
  }

  utils::SetEnv("NEOReadDebugKeys", "1");
  utils::SetEnv("UseCyclesPerSecondTimer", "1");

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (list_mode) {
    PrintDeviceList();
  } else {
    PrintDeviceInfo();
  }

  return 0;
}

#include <bitset>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "utils.h"
#include "ze_utils.h"

#define BYTES_IN_KB (1024.0f)
#define BYTES_IN_MB (1024.0f * 1024)
#define BYTES_IN_GB (1024.0f * 1024 * 1024)
#define W_IN_mW 1000

#define SPACES "    "
#define DEL_WIDTH 85
#define TEXT_WIDTH 40
#define UNKNOWN "unknown"

#define PID_LENGTH       8
#define MEMORY_LENGTH   24
#define ENGINES_LENGTH  12

enum Mode {
  MODE_PROCESSES,
  MODE_DEVICE_LIST,
  MODE_DETAILS
};

static void Usage() {
  std::cout <<
    "Usage: ./sysmon [options]" <<
    std::endl;
  std::cout << "Options:" << std::endl;
  std::cout <<
    "--processes [-p]    " <<
    "Print short device information and running processes (default)" <<
    std::endl;
  std::cout <<
    "--list [-l]         " <<
    "Print list of devices and subdevices" <<
    std::endl;
  std::cout <<
    "--details [-d]      " <<
    "Print detailed information for all of the devices and subdevices" <<
    std::endl;
  std::cout <<
    "--help [-h]         " <<
    "Print help message" <<
    std::endl;
  std::cout <<
    "--version           " <<
    "Print version" <<
    std::endl;
}

std::string ToString(double value) {
  std::ostringstream out;
  out.precision(1);
  out << std::fixed << value;
  return out.str();
}

static std::string GetDriverString(uint32_t version) {
  uint32_t major = version >> 24;
  uint32_t minor = (version >> 16) & 0xFF;
  uint32_t rev = version & 0xFFFF;
  return
    std::to_string(major) + "." +
    std::to_string(minor) + "." +
    std::to_string(rev);
}

static uint64_t GetDeviceMemSize(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t props_count = 0;
  status = zeDeviceGetMemoryProperties(device, &props_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (props_count == 0) {
    return 0;
  }

  std::vector<ze_device_memory_properties_t> props_list(
      props_count, {ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES, });
  status = zeDeviceGetMemoryProperties(device, &props_count, props_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint64_t total_mem_size = 0;
  for (auto& props : props_list) {
    total_mem_size += props.totalSize;
  }

  return total_mem_size;
}

static void PrintShorInfo(
    ze_driver_handle_t driver, zes_device_handle_t device,
    uint32_t device_id) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  // Save the original format state
  std::ios originalState(nullptr);
  originalState.copyfmt(std::cout);

  std::cout << std::setw(DEL_WIDTH) << std::setfill('=') << '=' << std::endl;

  zes_device_properties_t props{ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
  status = zesDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "GPU " << device_id << ": " << props.core.name;
  std::cout << SPACES;

  zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
  status = zesDevicePciGetProperties(device, &pci_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "PCI Bus: " << std::hex << std::setfill('0') <<
    std::setw(4) << pci_props.address.domain << ":" <<
    std::setw(2) << pci_props.address.bus << ":" <<
    std::setw(2) << pci_props.address.device << "." <<
    std::setw(1) << pci_props.address.function << std::dec << std::endl;

  ze_driver_properties_t driver_props{ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES, };
  status = zeDriverGetProperties(driver, &driver_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "Vendor: " << props.vendorName << SPACES;
  std::cout << "Driver Version: " <<
    GetDriverString(driver_props.driverVersion) << SPACES;
  std::cout << "Subdevices: " << props.numSubdevices << std::endl;

  uint32_t eu_count =
    props.core.numSlices *
    props.core.numSubslicesPerSlice *
    props.core.numEUsPerSubslice;
  std::cout << "EU Count: " << eu_count << SPACES;
  std::cout << "Threads Per EU: " << props.core.numThreadsPerEU << SPACES;
  std::cout << "EU SIMD Width: " <<
    props.core.physicalEUSimdWidth << SPACES;

  std::cout << "Total Memory(MB): " << std::fixed << std::setprecision(3) <<
    ToString(GetDeviceMemSize(device) / BYTES_IN_MB) << std::endl;

  uint32_t domain_count = 0;
  status = zesDeviceEnumFrequencyDomains(device, &domain_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "Core Frequency(MHz): ";

  if (domain_count > 0) {
    std::vector<zes_freq_handle_t> domain_list(domain_count);
    status = zesDeviceEnumFrequencyDomains(
        device, &domain_count, domain_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < domain_count; ++i) {
      zes_freq_properties_t domain_props{
          ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
      status = zesFrequencyGetProperties(domain_list[i], &domain_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if (domain_props.type == ZES_FREQ_DOMAIN_GPU) {
        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE, };
        status = zesFrequencyGetState(domain_list[i], &state);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        double current_frequency =
          (state.actual < domain_props.min) ? domain_props.min : state.actual;
        std::cout << std::setprecision(1) <<
          current_frequency << " of " << domain_props.max;
        break;
      }
    }
  } else {
    std::cout << UNKNOWN;
  }

  std::cout << SPACES;

  std::cout << "Core Temperature(C): ";

  uint32_t sensor_count = 0;
  status = zesDeviceEnumTemperatureSensors(
      device, &sensor_count, nullptr);

  if (status == ZE_RESULT_SUCCESS && sensor_count > 0) {
    std::vector<zes_temp_handle_t> sensor_list(sensor_count);
    status = zesDeviceEnumTemperatureSensors(
        device, &sensor_count, sensor_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < sensor_count; ++i) {
      zes_temp_properties_t temp_props{
          ZES_STRUCTURE_TYPE_TEMP_PROPERTIES, };
      status = zesTemperatureGetProperties(sensor_list[i], &temp_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if (temp_props.type == ZES_TEMP_SENSORS_GPU) {
        double temperature = 0.0f;
        status = zesTemperatureGetState(sensor_list[i], &temperature);
        if (status != ZE_RESULT_SUCCESS) {
          std::cout << UNKNOWN;
        } else {
          std::cout << std::setprecision(1) << temperature;
        }
        break;
      }
    }
  } else {
    std::cout << UNKNOWN;
  }

  std::cout << std::endl;

  std::cout << std::setw(DEL_WIDTH) << std::setfill('=') << '=' << std::endl;

  // Restore the original format state
  std::cout.copyfmt(originalState);
}

static std::vector<zes_process_state_t> GetDeviceProcesses(
    zes_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t proc_count = 0;
  status = zesDeviceProcessesGetState(device, &proc_count, nullptr);
  if (status != ZE_RESULT_SUCCESS || proc_count == 0) {
    return std::vector<zes_process_state_t>();
  }

  std::vector<zes_process_state_t> state_list(proc_count);
  status = zesDeviceProcessesGetState(
      device, &proc_count, state_list.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  return state_list;
}

static std::string GetProcessName(uint32_t pid) {
  std::string process_name;
  std::string file_name = "/proc/" + std::to_string(pid) + "/cmdline";

  std::ifstream file;
  file.open(file_name.data());

  if (!file.is_open()) {
    return std::string();
  }

  std::getline(file, process_name, '\0');
  file.close();

  return process_name;
}

static std::string GetEnginesString (uint64_t engines) {
  std::string engines_string;
  std::bitset<6> bits(engines);
  std::vector<std::string> engine_flags = {
    "OTHER", "COMPUTE", "3D", "MEDIA", "DMA", "RENDER"};

  if (engines == 0) {
    return "UNKNOWN";
  }

  for (size_t i = 0; i < bits.size(); ++i) {
    if (bits[i]) {
      engines_string += engine_flags[i] + ";";
    }
  }

  engines_string.pop_back();
  return engines_string;
}

static void PrintProcesses(zes_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  std::cout << "Running Processes: ";

  std::vector<zes_process_state_t> state_list = GetDeviceProcesses(device);
  if (state_list.empty()) {
    std::cout << UNKNOWN << std::endl;
    return;
  }

  std::cout << std::to_string(state_list.size()) << std::endl;

  uint32_t engines_length = ENGINES_LENGTH;
  for (auto& state : state_list) {
    std::string engines = GetEnginesString(state.engines);
    if (engines.size() > engines_length) {
      engines_length = engines.size();
    }
  }

  ++engines_length;

  std::cout << std::setfill(' ') <<
    std::setw(PID_LENGTH) << "PID" << "," <<
    std::setw(MEMORY_LENGTH) << "Device Memory Used(MB)" << "," <<
    std::setw(MEMORY_LENGTH) << "Shared Memory Used(MB)" << "," <<
    std::setw(engines_length) << "GPU Engines" << "," <<
    std::setw(1) << " Executable" << std::endl;

  for (auto& state : state_list) {
    std::cout <<
      std::setw(PID_LENGTH) << state.processId << "," <<
      std::setw(MEMORY_LENGTH) <<
        ToString(state.memSize / BYTES_IN_MB) << "," <<
      std::setw(MEMORY_LENGTH) <<
        ToString(state.sharedSize / BYTES_IN_MB) << "," <<
      std::setw(engines_length) << GetEnginesString(state.engines) << "," <<
      std::setw(1) << " " << GetProcessName(state.processId) << std::endl;
  }
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
      ze_device_properties_t device_properties{
          ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      status = zeDeviceGetProperties(device_list[j], &device_properties);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      std::cout << "-- Device #" << j << ": " <<
        device_properties.name << std::endl;

      std::vector<ze_device_handle_t> sub_device_list =
        utils::ze::GetSubDeviceList(device_list[j]);
      if (sub_device_list.empty()) {
        continue;
      }

      for (size_t k = 0; k < sub_device_list.size(); ++k) {
        ze_device_properties_t sub_device_properties{
            ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
        status = zeDeviceGetProperties(
            sub_device_list[k], &sub_device_properties);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::cout << "---- Subdevice #" << k << ": " <<
          sub_device_properties.name << std::endl;
      }
    }
  }
}

void PrintDeviceInfo(
    ze_driver_handle_t driver,
    ze_device_handle_t device) {
  PTI_ASSERT(driver != nullptr);
  PTI_ASSERT(device != nullptr);

  ze_structure_type_t stype =
      utils::ze::GetDriverVersion(driver) >= ZE_API_VERSION_1_2 ?
      ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2 :
      ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;

  ze_device_properties_t props{stype, };
  ze_result_t status = zeDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(TEXT_WIDTH) <<
    std::left << "Name," << props.name << std::endl;

  std::cout << std::setw(TEXT_WIDTH) <<
    std::left << "Vendor ID," << std::hex <<
    props.vendorId << std::dec << std::endl;

  std::cout << std::setw(TEXT_WIDTH) <<
    std::left << "Device ID," << std::hex <<
    props.deviceId << std::dec << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Core Clock Rate(MHz)," << props.coreClockRate << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Number Of Slices," << props.numSlices << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Number Of Subslices Per Slice," <<
    props.numSubslicesPerSlice << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Number Of EU Per Subslice," <<
    props.numEUsPerSubslice << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Number Of Threads Per EU," <<
    props.numThreadsPerEU << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Total EU Count," <<
    props.numSlices * props.numSubslicesPerSlice *
    props.numEUsPerSubslice << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Physical EU SIMD Width," <<
    props.physicalEUSimdWidth << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Kernel Timestamp Valid Bits," <<
    props.kernelTimestampValidBits << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Command Queue Priority," <<
    props.maxCommandQueuePriority << std::endl;

  std::string timer_resolution = "Timer Resolution(";
  timer_resolution +=
    utils::ze::GetDriverVersion(driver) < ZE_API_VERSION_1_2 ? "ns" : "clks";
  timer_resolution += "),";
  std::cout << std::setw(TEXT_WIDTH) <<
    timer_resolution << props.timerResolution << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Timestamp Valid Bits," <<
    props.timestampValidBits << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Hardware Contexts," <<
    props.maxHardwareContexts << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Memory Allocation Size(MB)," <<
    ToString(props.maxMemAllocSize / BYTES_IN_MB) << std::endl;
}

void PrintComputeInfo(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);

  ze_device_compute_properties_t props{
    ZE_STRUCTURE_TYPE_DEVICE_COMPUTE_PROPERTIES, };
  ze_result_t status = zeDeviceGetComputeProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Total Group Size," << props.maxTotalGroupSize << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Group Size X," << props.maxGroupSizeX << std::endl;
  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Group Size Y," << props.maxGroupSizeY << std::endl;
  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Group Size Z," << props.maxGroupSizeZ << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Group Count X," << props.maxGroupCountX << std::endl;
  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Group Count Y," << props.maxGroupCountY << std::endl;
  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Group Count Z," << props.maxGroupCountZ << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Shared Local Memory(KB)," <<
    ToString(props.maxSharedLocalMemory / BYTES_IN_KB) << std::endl;

  if (props.numSubGroupSizes > 0) {
    PTI_ASSERT(props.numSubGroupSizes <= ZE_SUBGROUPSIZE_COUNT);
    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      "Subgroup Sizes Supported,";
    for (uint32_t i = 0; i < props.numSubGroupSizes - 1; ++i) {
      std::cout << props.subGroupSizes[i] << ";";
    }
    std::cout << props.subGroupSizes[props.numSubGroupSizes - 1] << std::endl;
  }
}

void PrintFloatingPointFlags(
    const std::string& type, ze_device_fp_flags_t value) {
  static const std::pair<ze_device_fp_flag_t, const char *> flags[] = {
    {ZE_DEVICE_FP_FLAG_DENORM, "Denormals,"},
    {ZE_DEVICE_FP_FLAG_INF_NAN, "Infinity And NaN,"},
    {ZE_DEVICE_FP_FLAG_ROUND_TO_NEAREST, "Round To Nearest Even,"},
    {ZE_DEVICE_FP_FLAG_ROUND_TO_ZERO, "Round To Zero,"},
    {ZE_DEVICE_FP_FLAG_ROUND_TO_INF, "Round To Infinity,"},
    {ZE_DEVICE_FP_FLAG_FMA, "IEEE754-2008 FMA,"},
    {ZE_DEVICE_FP_FLAG_ROUNDED_DIVIDE_SQRT, "Correctly-Rounded Div Sqrt,"},
    {ZE_DEVICE_FP_FLAG_SOFT_FLOAT, "Emulated In Software,"},
  };

  for (auto &v: flags) {
    ze_device_fp_flag_t flag = v.first;
    std::string message = type + " " + v.second;
    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      message << (value & flag ? "yes" : "no") << std::endl;
  }
}

void PrintModuleInfo(ze_device_handle_t device) {
  PTI_ASSERT(device != nullptr);

  ze_device_module_properties_t props{
    ZE_STRUCTURE_TYPE_DEVICE_MODULE_PROPERTIES, };
  ze_result_t status = zeDeviceGetModuleProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "SPIR-V Version Supported," <<
    ZE_MAJOR_VERSION(props.spirvVersionSupported) << "." <<
    ZE_MINOR_VERSION(props.spirvVersionSupported) << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Device Module Flags,";
  if (props.flags == 0) {
    std::cout << "none" << std::endl;
  } else {
    std::string flags;
    if (props.flags & ZE_DEVICE_MODULE_FLAG_FP16) {
      flags += "fp16;";
    }
    if (props.flags & ZE_DEVICE_MODULE_FLAG_FP64) {
      flags += "fp64;";
    }
    if (props.flags & ZE_DEVICE_MODULE_FLAG_INT64_ATOMICS) {
      flags += "int64_atomics;";
    }
    if (props.flags & ZE_DEVICE_MODULE_FLAG_DP4A) {
      flags += "dp4a;";
    }
    PTI_ASSERT(!flags.empty());
    flags.pop_back();
    std::cout << flags << std::endl;
  }

  PrintFloatingPointFlags("FP16", props.fp16flags);
  PrintFloatingPointFlags("FP32", props.fp32flags);
  PrintFloatingPointFlags("FP64", props.fp64flags);

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Kernel Arguments Size(bytes)," <<
    props.maxArgumentsSize << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Max Print Buffer Size(KB)," <<
    ToString(props.printfBufferSize / BYTES_IN_KB) << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left << "Native Kernel UUID,";
  std::cout << std::hex << std::setfill('0');
  for (uint32_t i = 0; i < ZE_MAX_NATIVE_KERNEL_UUID_SIZE; ++i) {
    std::cout << std::setw(2) <<
      static_cast<uint16_t>(props.nativeKernelSupported.id[i]);
  }
  std::cout << std::setfill(' ') << std::dec << std::endl;
}

void PrintFrequencyInfo(
    zes_device_handle_t device,
    uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t freq_domain_count = 0;

  status = zesDeviceEnumFrequencyDomains(device, &freq_domain_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (freq_domain_count > 0) {
    std::vector<zes_freq_handle_t> freq_domain_list(freq_domain_count);
    status = zesDeviceEnumFrequencyDomains(
        device, &freq_domain_count, freq_domain_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::vector<zes_freq_handle_t> GPU_freq_domains;

    for (uint32_t i = 0; i < freq_domain_count; ++i) {
      zes_freq_properties_t freq_domain_props{
          ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
      status = zesFrequencyGetProperties(
          freq_domain_list[i], &freq_domain_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if (freq_domain_props.type == ZES_FREQ_DOMAIN_GPU) {
        GPU_freq_domains.push_back(freq_domain_list[i]);
      }
    }

    for (uint32_t i = 0; i < GPU_freq_domains.size(); ++i) {
      zes_freq_properties_t freq_domain_props{
          ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
      status = zesFrequencyGetProperties(
          GPU_freq_domains[i], &freq_domain_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE, };
      status = zesFrequencyGetState(GPU_freq_domains[i], &state);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((freq_domain_props.onSubdevice &&
          freq_domain_props.subdeviceId == subdevice_id) ||
          (!freq_domain_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        double current_frequency =
          (state.actual < freq_domain_props.min) ?
          freq_domain_props.min : state.actual;

        std::cout << std::setw(TEXT_WIDTH) << "Current Frequency(MHz)," <<
          current_frequency << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << "Changeable Frequency," <<
          (freq_domain_props.canControl ? "Yes" : "No") << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << "Max Core Frequency(MHz)," <<
          freq_domain_props.max << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << "Min Core Frequency(MHz)," <<
          freq_domain_props.min << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << "Current Voltage(V)," <<
          (state.currentVoltage < 0 ? UNKNOWN :
          ToString(state.currentVoltage)) << std::endl;

        std::cout << std::setw(TEXT_WIDTH) <<
          "Current Frequency Request(MHz)," <<
          (state.request < 0 ? UNKNOWN :
          ToString(state.request)) << std::endl;

        std::cout << std::setw(TEXT_WIDTH) <<
          "Efficient Min Frequency(MHz)," <<
          (state.efficient < 0 ? UNKNOWN :
          ToString(state.efficient)) << std::endl;

        std::cout << std::setw(TEXT_WIDTH) <<
          "Max Frequency For Current TDP(MHz)," <<
          (state.tdp < 0 ? UNKNOWN : ToString(state.tdp)) <<
          std::endl;
      }
    }
  }
}

void PrintPowerInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t power_domain_count = 0;
  status = zesDeviceEnumPowerDomains(device, &power_domain_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (power_domain_count > 0) {
    std::vector<zes_pwr_handle_t> power_domain_list(power_domain_count);
    status = zesDeviceEnumPowerDomains(
        device, &power_domain_count, power_domain_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < power_domain_count; ++i) {
      zes_power_properties_t power_domain_props{
          ZES_STRUCTURE_TYPE_POWER_PROPERTIES, };
      status = zesPowerGetProperties(
          power_domain_list[i], &power_domain_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((power_domain_props.onSubdevice  &&
          power_domain_props.subdeviceId == subdevice_id) ||
          (!power_domain_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Default TDP Power Limit (W)," <<
          (power_domain_props.defaultLimit == -1 ? UNKNOWN :
          ToString(power_domain_props.defaultLimit / W_IN_mW)) <<
          std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Changeable Power Limit," <<
          (power_domain_props.canControl ? "Yes" : "No") << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Max TDP Power Limit(W)," <<
          (power_domain_props.maxLimit == -1 ? UNKNOWN :
          ToString(power_domain_props.maxLimit / W_IN_mW)) << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Min TDP Power Limit(W)," <<
          (power_domain_props.minLimit == -1 ? UNKNOWN :
          ToString(power_domain_props.minLimit / W_IN_mW)) << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Supports Energy Threshold Event," <<
          (power_domain_props.isEnergyThresholdSupported ? "Yes" : "No") <<
          std::endl;
      }
    }
  }
}

void PrintFirmwareInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t firmwares_count = 0;
  status = zesDeviceEnumFirmwares(device, &firmwares_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (firmwares_count > 0) {
    std::vector<zes_firmware_handle_t> firmwares(firmwares_count);
    status = zesDeviceEnumFirmwares(
        device, &firmwares_count, firmwares.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < firmwares_count; ++i) {
      zes_firmware_properties_t firmwares_props{
          ZES_STRUCTURE_TYPE_FIRMWARE_PROPERTIES, };
      status = zesFirmwareGetProperties(firmwares[i], &firmwares_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((firmwares_props.onSubdevice  &&
          firmwares_props.subdeviceId == subdevice_id) ||
          (!firmwares_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Firmware Name," << firmwares_props.name << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Flashing Firmware," <<
          (firmwares_props.canControl ? "Yes" : "No") << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Firmware Version," << firmwares_props.version << std::endl;
      }
    }
  }
}

void PrintMemoryInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  if (subdevice_id == UINT32_MAX) {
    uint32_t mem_props_count = 0;
    status = zeDeviceGetMemoryProperties(device, &mem_props_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (mem_props_count > 0) {
      std::vector<ze_device_memory_properties_t> mem_props_list(
          mem_props_count, {ZE_STRUCTURE_TYPE_DEVICE_MEMORY_PROPERTIES, });
      status = zeDeviceGetMemoryProperties(
          device, &mem_props_count, mem_props_list.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      for (uint32_t i = 0; i < mem_props_count; ++i) {
        PTI_ASSERT(mem_props_list[i].name != nullptr);
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Memory Name," <<
          mem_props_list[i].name << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Memory Max Clock Rate(MHz)," <<
          mem_props_list[i].maxClockRate << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Memory Max Bus Width," <<
          mem_props_list[i].maxBusWidth << std::endl;

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          "Memory Total Size(MB)," <<
          ToString(mem_props_list[i].totalSize / BYTES_IN_MB) << std::endl;
      }
    }
  }

  uint32_t mem_modules_count = 0;
  status = zesDeviceEnumMemoryModules(device, &mem_modules_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::map<int, std::string> mem_health_types = {
    {ZES_MEM_HEALTH_UNKNOWN, "UNKNOWN"},
    {ZES_MEM_HEALTH_OK, "OK"},
    {ZES_MEM_HEALTH_DEGRADED, "DEGRADED"},
    {ZES_MEM_HEALTH_CRITICAL, "CRITICAL"},
    {ZES_MEM_HEALTH_REPLACE, "REPLACE"},
    {ZES_MEM_HEALTH_FORCE_UINT32, "FORCE_UINT32"}
  };

  std::map<int, std::string> mem_location = {
    {ZES_MEM_LOC_SYSTEM, "SYSTEM"},
    {ZES_MEM_LOC_DEVICE, "DEVICE"},
    {ZES_MEM_LOC_FORCE_UINT32, "FORCE_UINT32"}
  };

  std::map<int, std::string> mem_types = {
    {ZES_MEM_TYPE_HBM, "HBM"},
    {ZES_MEM_TYPE_DDR, "DDR"},
    {ZES_MEM_TYPE_DDR3, "DDR3"},
    {ZES_MEM_TYPE_DDR4, "DDR4"},
    {ZES_MEM_TYPE_DDR5, "DDR5"},
    {ZES_MEM_TYPE_LPDDR, "LPDDR"},
    {ZES_MEM_TYPE_LPDDR3, "LPDDR3"},
    {ZES_MEM_TYPE_LPDDR4, "LPDDR4"},
    {ZES_MEM_TYPE_LPDDR5, "LPDDR5"},
    {ZES_MEM_TYPE_SRAM, "SRAM"},
    {ZES_MEM_TYPE_L1, "L1"},
    {ZES_MEM_TYPE_L3, "L3"},
    {ZES_MEM_TYPE_GRF, "GRF"},
    {ZES_MEM_TYPE_SLM, "SLM"},
    {ZES_MEM_TYPE_FORCE_UINT32, "FORCE_UINT32"}
  };

  if (mem_modules_count > 0) {
    std::vector<zes_mem_handle_t> mem_modules(mem_modules_count);
    status = zesDeviceEnumMemoryModules(
        device, &mem_modules_count, mem_modules.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < mem_modules_count; ++i) {
      zes_mem_properties_t mem_props{
          ZES_STRUCTURE_TYPE_MEM_PROPERTIES, };
      status = zesMemoryGetProperties(mem_modules[i], &mem_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((mem_props.onSubdevice  &&
          mem_props.subdeviceId == subdevice_id) ||
          (!mem_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Memory Type," << mem_types[mem_props.type] << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Memory Location," << mem_location[mem_props.location] << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Memory Bus Width," <<
          (mem_props.busWidth == -1 ? UNKNOWN :
          std::to_string(mem_props.busWidth)) << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Memory Channels," <<
          (mem_props.numChannels == -1 ? UNKNOWN :
          std::to_string(mem_props.numChannels)) << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Physical Memory Size(MB)," <<
          (mem_props.physicalSize == 0 ? UNKNOWN :
          ToString(mem_props.physicalSize / BYTES_IN_MB)) << std::endl;

        zes_mem_state_t mem_state{ZES_STRUCTURE_TYPE_MEM_STATE, };
        status = zesMemoryGetState(mem_modules[i], &mem_state);
        if (status == ZE_RESULT_SUCCESS) {
          std::cout << std::left << std::setw(TEXT_WIDTH) <<
            "Free Memory(MB)," <<
            ToString(mem_state.free / BYTES_IN_MB) << std::endl;

          std::cout << std::left << std::setw(TEXT_WIDTH) <<
            "Total Allocatable Memory(MB)," <<
            ToString(mem_state.size / BYTES_IN_MB) << std::endl;

          std::cout << std::left << std::setw(TEXT_WIDTH) <<
            "Memory Health," << mem_health_types[mem_state.health] << std::endl;
        }
      }
    }
  }
}

void PrintEngineInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t engine_groups_count = 0;
  status = zesDeviceEnumEngineGroups(device, &engine_groups_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::map<int, std::string> engine_types = {
    {ZES_ENGINE_GROUP_ALL,"ALL"},
    {ZES_ENGINE_GROUP_COMPUTE_ALL, "COMPUTE_ALL"},
    {ZES_ENGINE_GROUP_MEDIA_ALL, "MEDIA_ALL"},
    {ZES_ENGINE_GROUP_COPY_ALL, "COPY_ALL"},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, "COMPUTE_SINGLE"},
    {ZES_ENGINE_GROUP_RENDER_SINGLE, "RENDER_SINGLE"},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE,
    "MEDIA_DECODE_SINGLE"},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE,
    "MEDIA_ENCODE_SINGLE"},
    {ZES_ENGINE_GROUP_COPY_SINGLE, "COPY_SINGLE"},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE,
    "MEDIA_ENHANCEMENT_SINGLE"},
    {ZES_ENGINE_GROUP_3D_SINGLE, "3D_SINGLE"},
    {ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL,
    "3D_RENDER_COMPUTE_ALL"},
    {ZES_ENGINE_GROUP_RENDER_ALL, "GROUP_RENDER_ALL"},
    {ZES_ENGINE_GROUP_3D_ALL, "3D_ALL"},
    {ZES_ENGINE_GROUP_FORCE_UINT32, "FORCE_UINT32"}
  };

  // Save the original format state
  std::ios originalState(nullptr);
  originalState.copyfmt(std::cout);

  if (engine_groups_count > 0) {
    std::vector<zes_engine_handle_t> engine_groups(engine_groups_count);
    status = zesDeviceEnumEngineGroups(
        device, &engine_groups_count, engine_groups.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::map<zes_engine_group_t, uint32_t> engine_map;

    for (uint32_t i = 0; i < engine_groups_count; ++i) {
      zes_engine_properties_t engine_props{
          ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES, };
      status = zesEngineGetProperties(engine_groups[i], &engine_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((engine_props.onSubdevice  &&
          engine_props.subdeviceId == subdevice_id) ||
          (!engine_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        if (engine_map.count(engine_props.type) == 0) {
          engine_map[engine_props.type] = 1;
        } else {
          engine_map[engine_props.type] += 1;
        }
      }
    }

    if (!engine_map.empty()) {
      std::cout << std::left << std::setw(TEXT_WIDTH) << "Engines,";

      std::string engines;
      for (auto items : engine_map) {
        engines += engine_types[items.first] + "(" +
          std::to_string(items.second) + ");";
      }

      engines.pop_back();
      std::cout << engines << std::endl;
    }
  }

  // Restore the original format state
  std::cout.copyfmt(originalState);
}

void PrintFabricPortInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t fabric_ports_count = 0;
  status = zesDeviceEnumFabricPorts(device, &fabric_ports_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (fabric_ports_count > 0) {
    std::vector<zes_fabric_port_handle_t> farbic_ports(fabric_ports_count);
    status = zesDeviceEnumFabricPorts(
        device, &fabric_ports_count, farbic_ports.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < fabric_ports_count; ++i) {
      zes_fabric_port_properties_t fabric_port_props{
          ZES_STRUCTURE_TYPE_FABRIC_PORT_PROPERTIES, };
      status = zesFabricPortGetProperties(farbic_ports[i], &fabric_port_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((fabric_port_props.onSubdevice  &&
          fabric_port_props.subdeviceId == subdevice_id) ||
          (!fabric_port_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Fabric Port ID," <<
          fabric_port_props.portId.fabricId << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Device Attachment Point ID," <<
          fabric_port_props.portId.attachId << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Logical Port Number," <<
          fabric_port_props.portId.portNumber << std::endl;
      }
    }
  }
}

void PrintFanInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t fan_count = 0;
  status = zesDeviceEnumFans(device, &fan_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (fan_count > 0) {
    std::vector<zes_fan_handle_t> fans(fan_count);
    status = zesDeviceEnumFans(
        device, &fan_count, fans.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < fan_count; ++i) {
      zes_fan_properties_t fan_props{
          ZES_STRUCTURE_TYPE_FAN_PROPERTIES, };
      status = zesFanGetProperties(fans[i], &fan_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((fan_props.onSubdevice  &&
          fan_props.subdeviceId == subdevice_id) ||
          (!fan_props.onSubdevice && subdevice_id == UINT32_MAX)) {
        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Max Fan RPM," << fan_props.maxRPM << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Max Points In FanTable," <<
          fan_props.maxPoints << std::endl;

        std::cout << std::left << std::setw(TEXT_WIDTH) <<
          "Fan Can Control," <<
          (fan_props.canControl ? "Yes" : "No") << std::endl;
      }
    }
  }
}

void PrintTemperatureInfo(
    zes_device_handle_t device, uint32_t subdevice_id = UINT32_MAX) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t sensor_count = 0;
  status = zesDeviceEnumTemperatureSensors(
      device, &sensor_count, nullptr);

  if (status == ZE_RESULT_SUCCESS && sensor_count > 0) {
    std::vector<zes_temp_handle_t> sensor_list(sensor_count);
    status = zesDeviceEnumTemperatureSensors(
        device, &sensor_count, sensor_list.data());
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    for (uint32_t i = 0; i < sensor_count; ++i) {
      zes_temp_properties_t temp_props{
          ZES_STRUCTURE_TYPE_TEMP_PROPERTIES, };
      status = zesTemperatureGetProperties(sensor_list[i], &temp_props);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      if ((temp_props.onSubdevice &&
          temp_props.subdeviceId == subdevice_id) ||
          (!temp_props.onSubdevice && temp_props.subdeviceId == UINT32_MAX)) {
        if (temp_props.type == ZES_TEMP_SENSORS_GPU) {
          double temp = 0.0f;
          status = zesTemperatureGetState(sensor_list[i], &temp);
          std::cout << std::setw(TEXT_WIDTH) <<
            std::left << "Core Temperature(C),";
          if (status == ZE_RESULT_SUCCESS) {
            std::cout << temp << std::endl;
          } else {
            std::cout << "N/A" << std::endl;
          }
        }

        if (temp_props.type == ZES_TEMP_SENSORS_MEMORY) {
          double temp = 0.0f;
          status = zesTemperatureGetState(sensor_list[i], &temp);
          std::cout << std::setw(TEXT_WIDTH) << std::left <<
            "Memory Temperature(C),";
          if (status == ZE_RESULT_SUCCESS) {
            std::cout << temp << std::endl;
          } else {
            std::cout << "N/A" << std::endl;
          }
        }
      }
    }
  }
}

static void PrintSubdeviceDetails(
    zes_device_handle_t device, uint32_t subdevice_id) {
  PTI_ASSERT(device != nullptr);

  std::vector<zes_device_handle_t> subdevice_list =
    utils::ze::GetSubDeviceList(device);
  PTI_ASSERT(subdevice_id < subdevice_list.size());

  ze_device_properties_t props{
      ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
  ze_result_t status = zeDeviceGetProperties(
      subdevice_list[subdevice_id], &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(TEXT_WIDTH) <<
    std::left << "Name," << props.name << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Number Of Slices," << props.numSlices << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Number Of Subslices Per Slice," <<
    props.numSubslicesPerSlice << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Number Of EU Per Subslice," <<
    props.numEUsPerSubslice << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Number Of Threads Per EU," <<
    props.numThreadsPerEU << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Total EU Count," <<
    props.numSlices * props.numSubslicesPerSlice *
    props.numEUsPerSubslice << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << "Physical EU SIMD Width," <<
    props.physicalEUSimdWidth << std::endl;

  PrintFrequencyInfo(device, subdevice_id);
  PrintPowerInfo(device, subdevice_id);
  PrintFirmwareInfo(device, subdevice_id);
  PrintMemoryInfo(device, subdevice_id);
  PrintEngineInfo(device, subdevice_id);
  PrintFabricPortInfo(device, subdevice_id);
  PrintFanInfo(device, subdevice_id);
  PrintTemperatureInfo(device, subdevice_id);
}

static void PrintDetails(ze_driver_handle_t driver,
    zes_device_handle_t device, uint32_t device_id) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  zes_device_properties_t props{ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
  status = zesDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(DEL_WIDTH) << std::setfill('=') <<
    '=' << std::setfill(' ') << std::endl;
  std::cout << "GPU " << device_id << std::endl;
  std::cout << std::setw(DEL_WIDTH) << std::setfill('=') <<
    '=' << std::setfill(' ') << std::endl;

  PrintDeviceInfo(driver, device);
  PrintComputeInfo(device);
  PrintModuleInfo(device);

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Brand Number," << props.boardNumber << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Brand Name," << props.brandName << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Kernel Driver Version," << props.driverVersion << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Serial Number," << props.serialNumber << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Model Name," << props.modelName << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Vendor," << props.vendorName << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Subdevices," << props.numSubdevices << std::endl;

  zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
  status = zesDevicePciGetProperties(device, &pci_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(TEXT_WIDTH) << "PCI Bus," <<
    std::hex << std::setfill('0') <<
    std::setw(4) << pci_props.address.domain << ":" <<
    std::setw(2) << pci_props.address.bus << ":" <<
    std::setw(2) << pci_props.address.device << "." <<
    std::setw(1) << pci_props.address.function << std::dec <<
    std::setfill(' ')<< std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "PCI Generation," << ((pci_props.maxSpeed.gen == -1) ?
    UNKNOWN : std::to_string(pci_props.maxSpeed.gen)) << std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "PCI Max Brandwidth(GB/s)," <<
    ((pci_props.maxSpeed.maxBandwidth == -1) ? UNKNOWN :
    ToString(pci_props.maxSpeed.maxBandwidth / BYTES_IN_GB)) <<
    std::endl;

  std::cout << std::setw(TEXT_WIDTH) << std::left << "PCI Width," <<
    ((pci_props.maxSpeed.width == -1) ?
    UNKNOWN : std::to_string(pci_props.maxSpeed.width)) << std::endl;

  ze_driver_properties_t driver_props{
        ZE_STRUCTURE_TYPE_DRIVER_PROPERTIES, };
  status = zeDriverGetProperties(driver, &driver_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << std::setw(TEXT_WIDTH) << std::left <<
    "Level Zero GPU Driver Version," <<
    GetDriverString(driver_props.driverVersion) << std::endl;

  PrintFrequencyInfo(device);
  PrintPowerInfo(device);
  PrintFirmwareInfo(device);
  PrintMemoryInfo(device);
  PrintEngineInfo(device);
  PrintFabricPortInfo(device);
  PrintFanInfo(device);
  PrintTemperatureInfo(device);

  if (props.numSubdevices > 0) {
    for (size_t subdevice = 0; subdevice < props.numSubdevices; ++subdevice) {
      std::cout << std::setw(DEL_WIDTH) << std::setfill('-') <<
        '-' << std::setfill(' ') << std::endl;
      PrintSubdeviceDetails(device, subdevice);
    }
  }

  std::cout << std::endl;
}

int main(int argc, char* argv[]) {
  Mode mode = MODE_PROCESSES;
  ze_result_t status = ZE_RESULT_SUCCESS;

  if (argc > 1) {
    if (std::string(argv[1]) == "--help" ||
        std::string(argv[1]) == "-h") {
      Usage();
      return 0;
    } else if (std::string(argv[1]) == "--list" ||
               std::string(argv[1]) == "-l") {
      mode = MODE_DEVICE_LIST;
    } else if (std::string(argv[1]) == "--processes" ||
               std::string(argv[1]) == "-p") {
      mode = MODE_PROCESSES;
    } else if (std::string(argv[1]) == "--details" ||
               std::string(argv[1]) == "-d") {
      mode = MODE_DETAILS;
    } else if (std::string(argv[1]) == "--version") {
#ifdef PTI_VERSION
      std::cout << TOSTRING(PTI_VERSION) << std::endl;
#endif
      return 0;
    }
  }

  utils::SetEnv("ZES_ENABLE_SYSMAN", "1");

  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);


  PTI_ASSERT(mode == MODE_DEVICE_LIST || mode == MODE_DETAILS ||
             mode == MODE_PROCESSES);
  switch(mode) {
    case MODE_DEVICE_LIST: {
      PrintDeviceList();
      break;
    }
    case MODE_DETAILS:
    case MODE_PROCESSES: {
      uint32_t device_id = 0;
      for (auto driver : utils::ze::GetDriverList()) {
        for (auto device : utils::ze::GetDeviceList(driver)) {
          if(mode == MODE_PROCESSES) {
            PrintShorInfo(driver, device, device_id);
            PrintProcesses(device);
          } else {
            PrintDetails(driver, device, device_id);
          }
          ++device_id;
        }
      }
      break;
    }
  }

  return 0;
}
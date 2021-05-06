#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <bitset>

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "utils.h"
#include "ze_utils.h"

#define BYTES_IN_MB (1024.0f * 1024.0f)
#define SPACES "    "

#define PID_LENGTH       8
#define MEMORY_LENGTH   24
#define ENGINES_LENGTH  12

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

static void PrintShortDeviceInfo(
    zes_device_handle_t device, uint32_t device_id) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  std::cout << std::setw(100) << std::setfill('=') << '=' << std::endl;

  zes_device_properties_t props{ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
  status = zesDeviceGetProperties(device, &props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "GPU " << device_id << ": " << props.core.name;
  std::cout << SPACES;

  zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
  status = zesDevicePciGetProperties(device, &pci_props);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::cout << "PCI bus: " << std::hex << std::setfill('0') <<
    std::setw(4) << pci_props.address.domain << ":" <<
    std::setw(2) << pci_props.address.bus << ":" <<
    std::setw(2) << pci_props.address.device << "." <<
    std::setw(1) << pci_props.address.function << std::dec << std::endl;

  std::cout << "Vendor: " << props.vendorName << SPACES;
  std::cout << "Driver version: " << props.driverVersion << SPACES;
  std::cout << "Subdevices: " << props.numSubdevices << std::endl;

  uint32_t eu_count =
    props.core.numSlices *
    props.core.numSubslicesPerSlice *
    props.core.numEUsPerSubslice;
  std::cout << "EU Count: " << eu_count << SPACES;
  std::cout << "Threads per EU: " << props.core.numThreadsPerEU << SPACES;
  std::cout << "EU SIMD Width: " <<
    props.core.physicalEUSimdWidth << SPACES;

  std::cout << "Total Memory(MB): " << std::fixed << std::setprecision(3) <<
    GetDeviceMemSize(device) / BYTES_IN_MB << std::endl;

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
    std::cout << "unknown";
  }

  std::cout << SPACES;

  std::cout << "Core Temperature(C): ";

  uint32_t sensor_count = 0;
  status = zesDeviceEnumTemperatureSensors(
      device, &sensor_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  if (sensor_count > 0) {
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
          std::cout << "unknown";
        } else {
          std::cout << std::setprecision(1) << temperature;
        }
        break;
      }
    }
  } else {
    std::cout << "unknown";
  }

  std::cout << std::endl;

  std::cout << std::setw(100) << std::setfill('=') << '=' << std::endl;
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

  if (bits[0]) {
    engines_string = engine_flags[0];
  }
  for (size_t i = 1; i < bits.size(); ++i) {
    if (bits[i]) {
      engines_string += "|" + engine_flags[i];
    }
  }

  return engines_string;
}

static void PrintProcesses(zes_device_handle_t device) {
  PTI_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  std::vector<zes_process_state_t> state_list = GetDeviceProcesses(device);
  if (state_list.empty()) {
    std::cout << "Running Processes: unknown" << std::endl;
    return;
  }

  std::cout << "Running Processes: " <<
    std::to_string(state_list.size()) << std::endl;

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
      std::setw(MEMORY_LENGTH) << std::fixed << std::setprecision(3) <<
        state.memSize / BYTES_IN_MB << "," <<
      std::setw(MEMORY_LENGTH) << std::fixed << std::setprecision(3) <<
        state.sharedSize / BYTES_IN_MB << "," <<
      std::setw(engines_length) << GetEnginesString(state.engines) << "," <<
      std::setw(1) << " " << GetProcessName(state.processId) << std::endl;
  }
}

int main(int argc, char* argv[]) {
  ze_result_t status = ZE_RESULT_SUCCESS;

  utils::SetEnv("ZES_ENABLE_SYSMAN", "1");

  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  uint32_t device_id = 0;
  for (auto driver : utils::ze::GetDriverList()) {
    for (auto device : utils::ze::GetDeviceList(driver)) {
      PrintShortDeviceInfo(device, device_id);
      PrintProcesses(device);
      ++device_id;
    }
  }

  return 0;
}
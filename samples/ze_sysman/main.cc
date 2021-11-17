#include <iomanip>
#include <iostream>
#include <vector>

#ifdef __linux__
#include <unistd.h>
#include <sys/types.h>
#endif

#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>

#include "pti_assert.h"
#include "utils.h"
#include "ze_utils.h"

#define BYTES_IN_MB (1024 * 1024)

int main() {
  utils::SetEnv("ZES_ENABLE_SYSMAN", "1");

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_device_handle_t device = utils::ze::GetGpuDevice();
  if (device == nullptr) {
    std::cout << "[WARNING] GPU device was not found" << std::endl;
    return 0;
  }

  // Sysman Device Properties
  {
    zes_device_properties_t device_props{
        ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
    status = zesDeviceGetProperties(device, &device_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::cout << "Device: " << device_props.core.name << std::endl;
    std::cout << "-- Subdevice Count: " <<
      device_props.numSubdevices << std::endl;
    std::cout << "-- Driver Version: " <<
      device_props.driverVersion << std::endl;
  }

  // Sysman PCI Properties
  {
    zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
    status = zesDevicePciGetProperties(device, &pci_props);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    std::cout << "-- PCI Bus: " << std::hex << std::setfill('0') <<
      std::setw(4) << pci_props.address.domain << ":" <<
      std::setw(2) << pci_props.address.bus    << ":" <<
      std::setw(2) << pci_props.address.device << "." <<
      std::setw(1) << pci_props.address.function <<
      std::dec << std::setfill(' ') << std::endl;
  }

  // Sysman Memory Properties
  {
    uint32_t module_count = 0;
    status = zesDeviceEnumMemoryModules(device, &module_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (module_count > 0) {
      std::cout << "-- Memory Modules: " << module_count << std::endl;

      std::vector<zes_mem_handle_t> module_list(module_count);
      status = zesDeviceEnumMemoryModules(
          device, &module_count, module_list.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      for (uint32_t i = 0; i < module_count; ++i) {
        zes_mem_properties_t memory_props{ZES_STRUCTURE_TYPE_MEM_PROPERTIES, };
        status = zesMemoryGetProperties(module_list[i], &memory_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        std::cout << "---- [" << i << "] Module Capacity (MB): " <<
          memory_props.physicalSize / BYTES_IN_MB << std::endl;
      }
    }
  }

  // Core Frequency
  {
    uint32_t domain_count = 0;
    status = zesDeviceEnumFrequencyDomains(device, &domain_count, nullptr);
    PTI_ASSERT(status == ZE_RESULT_SUCCESS);

    if (domain_count > 0) {
      std::cout << "-- Frequency Domains: " << domain_count << std::endl;

      std::vector<zes_freq_handle_t> domain_list(domain_count);
      status = zesDeviceEnumFrequencyDomains(
          device, &domain_count, domain_list.data());
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      for (uint32_t i = 0; i < domain_count; ++i) {
        zes_freq_properties_t domain_props{
            ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
        status = zesFrequencyGetProperties(domain_list[i], &domain_props);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);

        std::cout << "---- [" << i << "] Clock EU Freq Range (MHz): " <<
          domain_props.min << " - " << domain_props.max <<
          (domain_props.canControl ? " (changeable)" :  " (unchangeable)") <<
          std::endl;

        zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE, };
        status = zesFrequencyGetState(domain_list[i], &state);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        std::cout << "---- [" << i << "] Current Clock EU Freq (MHz): " <<
          state.actual << std::endl;
      }
    }
  }

  // Core Temperature
  {
    uint32_t sensor_count = 0;
    status = zesDeviceEnumTemperatureSensors(
        device, &sensor_count, nullptr);

    if (status == ZE_RESULT_SUCCESS && sensor_count > 0) {
#ifdef __linux__
      if (geteuid() != 0) {
        std::cout << "Need to be root to see temperature" << std::endl;
        return 0;
      }
#endif
      std::cout << "-- Temperature Sensors: " << sensor_count << std::endl;

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
          PTI_ASSERT(status == ZE_RESULT_SUCCESS);
          std::cout << "---- [" << i << "] Core Temperature (C): " <<
            temperature << std::endl;
        }
      }
    }
  }

  return 0;
}
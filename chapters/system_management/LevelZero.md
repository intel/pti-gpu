# System Management for oneAPI Level Zero (Level Zero)
## Overview
Level Zero provides a separate section of APIs called [System Management](https://spec.oneapi.com/level-zero/latest/sysman/PROG.html) to monitor and control the power and performance of accelerator devices.

**Supported Runtimes**:
- [Intel(R) Graphics Compute Runtime for oneAPI Level Zero and OpenCL(TM) Driver](https://github.com/intel/compute-runtime)

**Supported OS**:
- Linux
- Windows

**Supported HW**:
- Intel(R) Processor Graphics GEN9+

**Needed Headers**:
- [zes_api.h](https://github.com/oneapi-src/level-zero/blob/master/include/zes_api.h)

**Needed Libraries**:
- oneAPI Level Zero [libraries](https://github.com/intel/compute-runtime)

## How To Use

All the actions with Level Zero should start after initialization:
```cpp
zeInit(ZE_INIT_FLAG_GPU_ONLY);
```

### Device Enumeration

To start monitor some particular accelerator device one need to enumerate all the devices available through Level Zero and choose target one. Enumeration code may look like the following:
```cpp
// Get number of Level Zero drivers
uint32_t driver_count = 0;
zeDriverGet(&driver_count, nullptr);

// Get list of drivers
std::vector<ze_driver_handle_t> driver_list(driver_count);
zeDriverGet(&driver_count, driver_list.data());

// For each driver get the list of supported devices
for (uint32_t i = 0; i < driver_count; ++i) {

  // Get number of devices for the driver
  uint32_t device_count = 0;
  zeDeviceGet(driver_list[i], &device_count, nullptr);

  // Get list of devices
  std::vector<ze_device_handle_t> device_list(device_count);
  zeDeviceGet(driver, &device_count, device_list.data());

  // For each device in the list check if it's GPU
  for (uint32_t j = 0; j < device_count; ++j) {
    ze_device_properties_t device_properties{
        ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
    zeDeviceGetProperties(device_list[j], &device_properties);
    if (device_properties.type == ZE_DEVICE_TYPE_GPU) {
      return device_list[j]; // Choose the first GPU device
    }
  }
}
```

### Device Monitoring

Sysman API takes `zes_device_handle_t` as an argument. To get device of this type one can simply cast `ze_device_handle_t` to it or pass `ze_device_handle_t` directly to the function (cast will happen implicitly):
```cpp
zes_device_handle_t sysman_device = static_cast<zes_device_handle_t>(device);
zesDeviceGetProperties(sysman_device, ...);
// OR
zesDeviceGetProperties(device, ...);
```

First of all, one can collect a lot of **static** device information, including device, PCI and memory properties, e.g.:
```cpp
// Sysman Device Properties
{
  zes_device_properties_t device_props{
      ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
  zesDeviceGetProperties(device, &device_props);

  std::cout << "Device: " << device_props.core.name << std::endl;
  std::cout << "-- Subdevice Count: " <<
    device_props.numSubdevices << std::endl;
  std::cout << "-- Driver Version: " <<
    device_props.driverVersion << std::endl;
}

// Sysman PCI Properties
{
  zes_pci_properties_t pci_props{ZES_STRUCTURE_TYPE_PCI_PROPERTIES, };
  zesDevicePciGetProperties(device, &pci_props);

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
  zesDeviceEnumMemoryModules(device, &module_count, nullptr);

  if (module_count > 0) {
    std::cout << "-- Memory Modules: " << module_count << std::endl;

    std::vector<zes_mem_handle_t> module_list(module_count);
    zesDeviceEnumMemoryModules(device, &module_count, module_list.data());

    for (uint32_t i = 0; i < module_count; ++i) {
      zes_mem_properties_t memory_props{ZES_STRUCTURE_TYPE_MEM_PROPERTIES, };
      zesMemoryGetProperties(module_list[i], &memory_props);

      std::cout << "---- [" << i << "] Module Capacity (MB): " <<
        memory_props.physicalSize / BYTES_IN_MB << std::endl;
    }
  }
}
```

Also it's possible to collect **dynamic** device information, like current core frequency, temperature, used memory size, etc.
```cpp
// Core Frequency
{
  uint32_t domain_count = 0;
  zesDeviceEnumFrequencyDomains(device, &domain_count, nullptr);

  if (domain_count > 0) {
    std::cout << "-- Frequency Domains: " << domain_count << std::endl;

    std::vector<zes_freq_handle_t> domain_list(domain_count);
    zesDeviceEnumFrequencyDomains(
        device, &domain_count, domain_list.data());

    for (uint32_t i = 0; i < domain_count; ++i) {
      zes_freq_properties_t domain_props{
          ZES_STRUCTURE_TYPE_FREQ_PROPERTIES, };
      zesFrequencyGetProperties(domain_list[i], &domain_props);

      std::cout << "---- [" << i << "] Clock EU Freq Range (MHz): " <<
        domain_props.min << " - " << domain_props.max <<
        (domain_props.canControl ? " (changeable)" :  " (unchangeable)") <<
        std::endl;

      zes_freq_state_t state{ZES_STRUCTURE_TYPE_FREQ_STATE, };
      zesFrequencyGetState(domain_list[i], &state);
      std::cout << "---- [" << i << "] Current Clock EU Freq (MHz): " <<
        state.actual << std::endl;
    }
  }
}

// Core Temperature (one need to be root to make it working)
{
  uint32_t sensor_count = 0;
  zesDeviceEnumTemperatureSensors(device, &sensor_count, nullptr);

  if (sensor_count > 0) {
    std::cout << "-- Temperature Sensors: " << sensor_count << std::endl;

    std::vector<zes_temp_handle_t> sensor_list(sensor_count);
    zesDeviceEnumTemperatureSensors(
        device, &sensor_count, sensor_list.data());

    for (uint32_t i = 0; i < sensor_count; ++i) {
      zes_temp_properties_t temp_props{
          ZES_STRUCTURE_TYPE_TEMP_PROPERTIES, };
      zesTemperatureGetProperties(sensor_list[i], &temp_props);

      if (temp_props.type == ZES_TEMP_SENSORS_GPU) {
        double temperature = 0.0f;
        zesTemperatureGetState(sensor_list[i], &temperature);
        std::cout << "---- [" << i << "] Core Temperature (C): " <<
          temperature << std::endl;
      }
    }
  }
}
```
In addition Level Zero Sysman API allows to retrieve set of processes currently using accelerator device (including thier memory consumption) through `zesDeviceProcessesGetState`. Look into [documentation](https://spec.oneapi.com/level-zero/latest/sysman/PROG.html) for more details.

### Device Control

Sysman API provides an ability to change some device parameters in addition to just retrieve its current value. E.g. for core frequency one may use the following code:
```cpp
/* One need to be root to make this code working */

// Get number of available frequency domains
uint32_t domain_count = 0;
zesDeviceEnumFrequencyDomains(device, &domain_count, nullptr);

// Get frequency domains
std::vector<zes_freq_handle_t> domain_list(domain_count);
zesDeviceEnumFrequencyDomains(device, &domain_count, domain_list.data());

// Set frequency range for each of the domains
for (uint32_t i = 0; i < domain_count; ++i) {
  zes_freq_range_t freq_range{300.0, 1200.0};
  zesFrequencySetRange(domain_list[i], freq_range);
}

```

## Build and Run
To monitor and control accelerator devices one need to link the application with Level Zero ICD library (e.g. `libze_loader.so`) and run it as following:
```
ZES_ENABLE_SYSMAN=1 ./<application>
```

## Usage Details
- refer to oneAPI Level Zero Sysman API [documentation](https://spec.oneapi.com/level-zero/latest/sysman/PROG.html) to learn more

## Samples
- [Level Zero System Management](../../samples/ze_sysman)
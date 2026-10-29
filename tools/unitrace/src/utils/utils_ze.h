//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_ZE_UTILS_H_
#define PTI_UTILS_ZE_UTILS_H_

#include <string.h>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>

#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>
#include "ze_loader.h"
#include "utils/utils_host.h"

#include "utils/demangle.h"
#include "logger_factory.h"
#include "utils/unitrace_assert.h"
#include "utils/utils.h"

inline bool InitializeL0() {
#if BUILD_WITH_L0
  auto status = ZE_FUNC(zeInit)(ZE_INIT_FLAG_GPU_ONLY);
  if (status != ZE_RESULT_SUCCESS) {
    std::cerr << "[ERROR] Failed to initialize Level Zero runtime" << std::endl;
#ifndef _WIN32
    std::cerr << "[INFO] Please ensure that either /proc/sys/dev/i915/perf_stream_paranoid or /proc/sys/dev/xe/observation_paranoid is set to 0." << std::endl;
#endif /* _WIN32 */
    return false;
  } else {
    return true;
  }
#else /* BUILD_WITH_L0 */
  return true;
#endif /* BUILD_WITH_L0 */
}

// Parses a comma-separated list of device ids (--devices-to-sample).
//   list  the list, e.g. "0,2"
//   out   receives the ids
// Returns 0 on success; -1 if list is empty or an item is not a decimal number that
// fits an int (an error is printed). Empty items ("0,,2") are skipped. Never
// throws.
inline int ParseDeviceList(const std::string& list, std::set<int>& out) {
  if (list.empty()) {
    return -1;
  }
  for (const auto& s : utils::SplitString(list, ',')) {
    if (!s.empty()) {
      int device = 0;
      if (IsDigits(s) && ToInt(s, device)) {
        out.insert(device);
      } else {
        std::cerr << "[ERROR] Given device to sample (" << s << ") is invalid" << std::endl;
        return -1;
      }
    }
  }
  return 0;
}

// Parses a comma-separated list of MPI ranks (--ranks-to-sample) against the
// launcher's environment.
//   list      the list, e.g. "0,3"
//   mpi_size  the job size (PMI_SIZE, PMIX_SIZE, OMPI_COMM_WORLD_SIZE, ...)
//   mpi_rank  this process's rank (PMI_RANK or PMIX_RANK)
//   out       receives the ranks in [0, mpi_size)
// Returns 0 on success; -1 if list is empty, the MPI environment is missing or not
// a number, or an item is not decimal digits (an error is printed; for an item,
// by rank 0 only). A rank outside [0, mpi_size), including one too large for an
// int, is ignored with a warning. Never throws.
inline int ParseRankList(const std::string& list, const std::string& mpi_size,
                         const std::string& mpi_rank, std::set<int>& out) {
  if (list.empty()) {
    return -1;
  }
  if (mpi_size.empty()) {
    std::cerr << "[ERROR] PMI_SIZE or PMIX_SIZE or OMPI_COMM_WORLD_SIZE or OMPI_UNIVERSE_SIZE not set. Given --ranks-to-sample but the application does not seem to be using MPI" << std::endl;
    return -1;
  }
  int32_t my_size = 0;
  if (!ToInt(mpi_size, my_size)) {
    std::cerr << "[ERROR] MPI size (" << mpi_size << ") is not a number" << std::endl;
    return -1;
  }
  if (mpi_rank.empty()) {
    std::cerr << "[ERROR] Given --ranks-to-sample but the application does not seem to be using MPI" << std::endl;
    return -1;
  }
  int32_t my_rank = 0;
  if (!ToInt(mpi_rank, my_rank)) {
    std::cerr << "[ERROR] MPI rank (" << mpi_rank << ") is not a number" << std::endl;
    return -1;
  }
  for (const auto& s : utils::SplitString(list, ',')) {
    if (!s.empty()) {
      int rank = -1;
      if (IsDigits(s)) {
        if (!ToInt(s, rank)) {
          rank = -1;  // too large to be a rank: out of bounds
        }
        if ((0 <= rank) && (rank < my_size)) {
          out.insert(rank);
        } else if (my_rank == 0) {
          std::cerr << "[WARNING] Given MPI rank to sample (" << s << ") is out of bounds for given execution. Ignoring." << std::endl;
        }
      } else if (my_rank == 0) {
        std::cerr << "[ERROR] Given MPI rank to sample (" << s << ") is invalid" << std::endl;
        return -1;
      }
    }
  }
  return 0;
}

// Parses an optional device index from the environment (UNITRACE_DeviceId,
// PTI_DEVICE_ID, PTI_SUB_DEVICE_ID).
//   value  the variable's value; empty means not set
//   out    receives the index: 0 when value is empty
// Returns false, with out untouched, if value is not decimal digits or does not fit
// a uint32_t; the caller reports the error instead of an exception escaping into
// the application.
inline bool ParseDeviceIndex(const std::string& value, uint32_t& out) {
  if (value.empty()) {
    out = 0;
    return true;
  }
  return ToUint32(value, out);
}

// The device ids of --devices-to-sample as the metric profiler reads them from its
// environment.
//   list  the --devices-to-sample value the profiler is created with
//   out   receives the ids; for an invalid list, only kNoDevice
// unitrace has already rejected an invalid list; one here (set by hand) samples no
// device rather than throwing inside the application. An empty set would mean "all
// devices" to the profiler, so an invalid list leaves an id no device has.
constexpr int kNoDevice = -1;
inline void DevicesStringToSet(const std::string& list, std::set<int>& out) {
  if (!list.empty() && ParseDeviceList(list, out) != 0) {
    std::cerr << "[ERROR] Devices to sample (" << list << ") are invalid; no device is sampled" << std::endl;
    out = {kNoDevice};
  }
}

inline std::vector<ze_driver_handle_t> GetDriverList() {
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t driver_count = 0;
  status = ZE_FUNC(zeDriverGet)(&driver_count, nullptr);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  if (driver_count == 0) {
    return std::vector<ze_driver_handle_t>();
  }

  std::vector<ze_driver_handle_t> driver_list(driver_count);
  status = ZE_FUNC(zeDriverGet)(&driver_count, driver_list.data());
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  return driver_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList(ze_driver_handle_t driver) {
  UNITRACE_ASSERT(driver != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t device_count = 0;
  status = ZE_FUNC(zeDeviceGet)(driver, &device_count, nullptr);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  if (device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> device_list(device_count);
  status = ZE_FUNC(zeDeviceGet)(driver, &device_count, device_list.data());
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  return device_list;
}

inline std::vector<ze_device_handle_t> GetDeviceList() {
  std::vector<ze_device_handle_t> device_list;
  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      device_list.push_back(device);
    }
  }
  return device_list;
}

inline std::vector<ze_device_handle_t> GetSubDeviceList(
    ze_device_handle_t device) {
  UNITRACE_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t sub_device_count = 0;
  status = ZE_FUNC(zeDeviceGetSubDevices)(device, &sub_device_count, nullptr);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  if (sub_device_count == 0) {
    return std::vector<ze_device_handle_t>();
  }

  std::vector<ze_device_handle_t> sub_device_list(sub_device_count);
  status = ZE_FUNC(zeDeviceGetSubDevices)(
      device, &sub_device_count, sub_device_list.data());
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  return sub_device_list;
}

inline ze_driver_handle_t GetGpuDriver() {
  std::vector<ze_driver_handle_t> driver_list;

  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      ze_result_t status = ZE_FUNC(zeDeviceGetProperties)(device, &props);
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
      if (props.type == ZE_DEVICE_TYPE_GPU) {
        driver_list.push_back(driver);
      }
    }
  }

  if (driver_list.empty()) {
    return nullptr;
  }

  std::string value = utils::GetEnv("PTI_DEVICE_ID");
  uint32_t device_id = 0;
  if (!ParseDeviceIndex(value, device_id)) {
    std::cerr << "[ERROR] PTI_DEVICE_ID (" << value << ") is invalid" << std::endl;
    return nullptr;
  }
  if (device_id >= driver_list.size()) {
    std::cerr << "[ERROR] PTI_DEVICE_ID (" << value << ") is out of range, " << driver_list.size() << " driver(s) found" << std::endl;
    return nullptr;
  }
  return driver_list[device_id];
}

inline ze_device_handle_t GetGpuDevice() {
  std::vector<ze_device_handle_t> device_list;

  for (auto driver : GetDriverList()) {
    for (auto device : GetDeviceList(driver)) {
      ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
      ze_result_t status = ZE_FUNC(zeDeviceGetProperties)(device, &props);
      UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
      if (props.type == ZE_DEVICE_TYPE_GPU) {
        device_list.push_back(device);
      }
    }
  }

  if (device_list.empty()) {
    return nullptr;
  }

  std::string value = utils::GetEnv("PTI_DEVICE_ID");
  uint32_t device_id = 0;
  if (!ParseDeviceIndex(value, device_id)) {
    std::cerr << "[ERROR] PTI_DEVICE_ID (" << value << ") is invalid" << std::endl;
    return nullptr;
  }
  if (device_id >= device_list.size()) {
    std::cerr << "[ERROR] PTI_DEVICE_ID (" << value << ") is out of range, " << device_list.size() << " GPU device(s) found" << std::endl;
    return nullptr;
  }

  std::vector<ze_device_handle_t> sub_device_list =
    GetSubDeviceList(device_list[device_id]);
  if (sub_device_list.empty()) {
    return device_list[device_id];
  }

  value = utils::GetEnv("PTI_SUB_DEVICE_ID");
  if (value.empty()) {
    return device_list[device_id];
  }

  uint32_t sub_device_id = 0;
  if (!ParseDeviceIndex(value, sub_device_id)) {
    std::cerr << "[ERROR] PTI_SUB_DEVICE_ID (" << value << ") is invalid" << std::endl;
    return nullptr;
  }
  if (sub_device_id >= sub_device_list.size()) {
    std::cerr << "[ERROR] PTI_SUB_DEVICE_ID (" << value << ") is out of range, " << sub_device_list.size() << " sub-device(s) found" << std::endl;
    return nullptr;
  }
  return sub_device_list[sub_device_id];
}

inline ze_context_handle_t GetContext(ze_driver_handle_t driver) {
  UNITRACE_ASSERT(driver != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_context_handle_t context = nullptr;
  ze_context_desc_t context_desc = {
      ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};

  status = ZE_FUNC(zeContextCreate)(driver, &context_desc, &context);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
  return context;
}

inline std::string GetDeviceName(ze_device_handle_t device) {
  UNITRACE_ASSERT(device != nullptr);
  ze_result_t status = ZE_RESULT_SUCCESS;
  ze_device_properties_t props{ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES, };
  status = ZE_FUNC(zeDeviceGetProperties)(device, &props);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.name;
}

inline int GetMetricId(zet_metric_group_handle_t group, std::string name) {
  UNITRACE_ASSERT(group != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t metric_count = 0;
  status = ZE_FUNC(zetMetricGet)(group, &metric_count, nullptr);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  if (metric_count == 0) {
    return -1;
  }

  std::vector<zet_metric_handle_t> metric_list(metric_count, nullptr);
  status = ZE_FUNC(zetMetricGet)(group, &metric_count, metric_list.data());
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  int target = -1;
  for (uint32_t i = 0; i < metric_count; ++i) {
    zet_metric_properties_t metric_props{};
    status = ZE_FUNC(zetMetricGetProperties)(metric_list[i], &metric_props);
    UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

    if (name == metric_props.name) {
      target = i;
      break;
    }
  }

  return target;
}

inline zet_metric_group_handle_t FindMetricGroup(
    ze_device_handle_t device, std::string name,
    zet_metric_group_sampling_type_flag_t type) {
  UNITRACE_ASSERT(device != nullptr);

  ze_result_t status = ZE_RESULT_SUCCESS;
  uint32_t group_count = 0;
  status = ZE_FUNC(zetMetricGroupGet)(device, &group_count, nullptr);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
  if (group_count == 0) {
    return nullptr;
  }

  std::vector<zet_metric_group_handle_t> group_list(group_count, nullptr);
  status = ZE_FUNC(zetMetricGroupGet)(device, &group_count, group_list.data());
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  zet_metric_group_handle_t target = nullptr;
  for (uint32_t i = 0; i < group_count; ++i) {
    zet_metric_group_properties_t group_props{};
    group_props.stype = ZET_STRUCTURE_TYPE_METRIC_GROUP_PROPERTIES;
    status = ZE_FUNC(zetMetricGroupGetProperties)(group_list[i], &group_props);
    UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

    if (name == group_props.name && (group_props.samplingType & type)) {
      target = group_list[i];
      break;
    }
  }

  return target;
}

inline std::string GetResultType(zet_value_type_t type) {
  switch (type) {
    case ZET_VALUE_TYPE_UINT32:
      return "UINT32";
    case ZET_VALUE_TYPE_UINT64:
      return "UINT64";
    case ZET_VALUE_TYPE_FLOAT32:
      return "FLOAT32";
    case ZET_VALUE_TYPE_FLOAT64:
      return "FLOAT64";
    case ZET_VALUE_TYPE_BOOL8:
      return "BOOL8";
    default:
      break;
  }
  return "UNKNOWN";
}

inline std::string GetMetricType(zet_metric_type_t type) {
  switch (type) {
    case ZET_METRIC_TYPE_DURATION:
      return "DURATION";
    case ZET_METRIC_TYPE_EVENT:
      return "EVENT";
    case ZET_METRIC_TYPE_EVENT_WITH_RANGE:
      return "EVENT_WITH_RANGE";
    case ZET_METRIC_TYPE_THROUGHPUT:
      return "THROUGHPUT";
    case ZET_METRIC_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case ZET_METRIC_TYPE_FLAG:
      return "FLAG";
    case ZET_METRIC_TYPE_RATIO:
      return "RATIO";
    case ZET_METRIC_TYPE_RAW:
      return "RAW";
    default:
      break;
  }
  return "UNKNOWN";
}

inline size_t GetKernelMaxSubgroupSize(ze_kernel_handle_t kernel) {
  UNITRACE_ASSERT(kernel != nullptr);
  ze_kernel_properties_t props{};
  ze_result_t status = ZE_FUNC(zeKernelGetProperties)(kernel, &props);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
  return props.maxSubgroupSize;
}

inline std::string GetKernelName(
    ze_kernel_handle_t kernel, bool demangle = false) {
  UNITRACE_ASSERT(kernel != nullptr);

  size_t size = 0;
  ze_result_t status = ZE_FUNC(zeKernelGetName)(kernel, &size, nullptr);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
  UNITRACE_ASSERT(size > 0);

  std::vector<char> name(size);
  status = ZE_FUNC(zeKernelGetName)(kernel, &size, name.data());
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
  UNITRACE_ASSERT(name[size - 1] == '\0');

  if (demangle) {
    return utils::Demangle(name.data());
  }
  return std::string(name.begin(), name.end() - 1);
}

inline void GetDeviceTimestamps(
    ze_device_handle_t device,
    uint64_t* host_timestamp,
    uint64_t* device_timestamp) {
  UNITRACE_ASSERT(device != nullptr);
  UNITRACE_ASSERT(host_timestamp != nullptr);
  UNITRACE_ASSERT(device_timestamp != nullptr);
  ze_result_t status = ZE_FUNC(zeDeviceGetGlobalTimestamps)(
      device, host_timestamp, device_timestamp);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
}

// TODO: use zeMetricGetGlobalTimestamps
inline void GetMetricTimestamps(
    ze_device_handle_t device,
    uint64_t* host_timestamp,
    uint64_t* metric_timestamp) {
  UNITRACE_ASSERT(device != nullptr);
  UNITRACE_ASSERT(host_timestamp != nullptr);
  UNITRACE_ASSERT(metric_timestamp != nullptr);
  ze_result_t status = ZE_FUNC(zeDeviceGetGlobalTimestamps)(
      device, host_timestamp, metric_timestamp);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);
}

inline ze_api_version_t GetDriverVersion(ze_driver_handle_t driver) {
  UNITRACE_ASSERT(driver != nullptr);

  ze_api_version_t version = ZE_API_VERSION_FORCE_UINT32;
  ze_result_t status = ZE_FUNC(zeDriverGetApiVersion)(driver, &version);
  UNITRACE_ASSERT(status == ZE_RESULT_SUCCESS);

  return version;
}

inline ze_api_version_t GetZeVersion() {
  auto driver_list = GetDriverList();
  if (driver_list.empty()) {
    return ZE_API_VERSION_FORCE_UINT32;
  }
  return GetDriverVersion(driver_list.front());
}

inline int GetZeDevicesToSample(std::set<int>& devices_to_sample) {
  return ParseDeviceList(utils::GetEnv("UNITRACE_DevicesToSampleArg"),
                                        devices_to_sample);
}

inline int GetZeRanksToSample(std::set<int>& ranks_to_sample) {
  auto my_MPI_size = (utils::GetEnv("PMI_SIZE").empty()) ? utils::GetEnv("PMIX_SIZE") : utils::GetEnv("PMI_SIZE");
  if (my_MPI_size.empty()) {
    my_MPI_size = (utils::GetEnv("OMPI_COMM_WORLD_SIZE").empty()) ? utils::GetEnv("OMPI_UNIVERSE_SIZE") : utils::GetEnv("OMPI_COMM_WORLD_SIZE");
  }
  const auto my_MPI_rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
  return ParseRankList(utils::GetEnv("UNITRACE_RanksToSample"), my_MPI_size,
                                      my_MPI_rank, ranks_to_sample);
}

inline void GetZeDevicesStringToSet(std::set<int>& device_ids_to_sample, std::string& devices_to_sample) {
  DevicesStringToSet(devices_to_sample, device_ids_to_sample);
}

// Write a module's native binary verbatim - the bytes zeModuleGetNativeBinary returns -
// as the file module_id names in data_dir, for unitrace's own process to read back. The
// collector's whole part in the module-binary half of the kernel handoff: it never reads
// one back, so no IGA, ELF or DWARF code is linked into the traced application. See
// ZeMetricProfiler::ReadModuleAndKernelProps, which documents the handoff.
//
// The path is built here and not by the caller, which has only the id and the bytes. Its
// counterpart, ReadBackModuleNativeBinary below, builds it from the same LoggerFactory:
// that is the one thing the two sides have to agree about, since the bytes pass through
// unaltered.
inline bool WriteOutModuleNativeBinary(const LoggerFactory* logger_factory,
                                       const std::string& data_dir, uint32_t module_id,
                                       const uint8_t* data, size_t size) {
  const std::string path =
      data_dir + "/" + logger_factory->GetModuleBinaryFileName(utils::GetPid(), module_id);

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    std::cerr << "[ERROR] Failed to open file for writing: " << path << std::endl;
    return false;
  }

  file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));

  // Check after close(), which flushes the last buffered write: a full disk shows
  // up there rather than on write(), and reporting success on a short file would
  // surface later as an undiagnosable "failed to disassemble".
  file.close();
  if (!file.good()) {
    std::cerr << "[ERROR] Error writing module binary to: " << path << std::endl;
    return false;
  }

  return true;
}

// Read back a dumped module binary.
inline bool ReadBackModuleNativeBinary(const std::string& path,
                                       std::vector<uint8_t>& out_binary) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    std::cerr << "[WARNING] Failed to open module binary file: " << path << std::endl;
    return false;
  }

  std::streampos size = file.tellg();
  if (size <= 0) {
    std::cerr << "[WARNING] Module binary file is empty or unreadable: " << path << std::endl;
    return false;
  }
  file.seekg(0, std::ios::beg);

  out_binary.resize(static_cast<size_t>(size));
  file.read(reinterpret_cast<char*>(out_binary.data()), size);
  if (file.gcount() != size) {
    std::cerr << "[WARNING] Module binary file is incomplete: " << path << std::endl;
    return false;
  }

  return true;
}
#endif // PTI_UTILS_ZE_UTILS_H_

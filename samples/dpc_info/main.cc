#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include <sycl/sycl.hpp>

#define TAB std::string("  ")

#define TEXT_WIDTH   50
#define BYTES_IN_KB  1024
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
    if (value % (BYTES_IN_MB != 0)) {
      std::stringstream tempstream;
      tempstream << std::fixed << std::setprecision(2) <<
        static_cast<float>(value) / BYTES_IN_MB << "MiB";
      return tempstream.str();
    }
    return std::to_string(value / BYTES_IN_MB) + "MiB";
  }
  if (value % (BYTES_IN_GB) != 0) {
    std::stringstream tempstream;
    tempstream << std::fixed << std::setprecision(2) <<
      static_cast<float>(value) / BYTES_IN_GB << "GiB";
    return tempstream.str();
  }
  return std::to_string(value / BYTES_IN_GB) + "GiB";
}

std::ostream& operator<<(std::ostream& out, sycl::aspect sycl_aspect) {
  switch (sycl_aspect) {
    case sycl::aspect::cpu:
      out << "cpu";
      break;
    case sycl::aspect::gpu:
      out << "gpu";
      break;
    case sycl::aspect::accelerator:
      out << "accelerator";
      break;
    case sycl::aspect::custom:
      out << "custom";
      break;
    case sycl::aspect::emulated:
      out << "emulated";
      break;
    case sycl::aspect::host_debuggable:
      out << "host_debuggable";
      break;
    case sycl::aspect::fp16:
      out << "fp16";
      break;
    case sycl::aspect::fp64:
      out << "fp64";
      break;
    case sycl::aspect::atomic64:
      out << "atomic64";
      break;
    case sycl::aspect::image:
      out << "image";
      break;
    case sycl::aspect::online_compiler:
      out << "online_compiler";
      break;
    case sycl::aspect::online_linker:
      out << "online_linker";
      break;
    case sycl::aspect::queue_profiling:
      out << "queue_profiling";
      break;
    case sycl::aspect::usm_device_allocations:
      out << "usm_device_allocations";
      break;
    case sycl::aspect::usm_host_allocations:
      out << "usm_host_allocations";
      break;
    case sycl::aspect::usm_atomic_host_allocations:
      out << "usm_atomic_host_allocations";
      break;
    case sycl::aspect::usm_shared_allocations:
      out << "usm_shared_allocations";
      break;
    case sycl::aspect::usm_atomic_shared_allocations:
      out << "usm_atomic_shared_allocations";
      break;
    case sycl::aspect::usm_system_allocations:
      out << "usm_system_allocations";
      break;
#if defined(SYCL_IMPLEMENTATION_INTEL)
    case sycl::aspect::ext_intel_pci_address:
      out << "ext_intel_pci_address";
      break;
    case sycl::aspect::ext_intel_gpu_eu_count:
      out << "ext_intel_gpu_eu_count";
      break;
    case sycl::aspect::ext_intel_gpu_eu_simd_width:
      out << "ext_intel_gpu_eu_simd_width";
      break;
    case sycl::aspect::ext_intel_gpu_slices:
      out << "ext_intel_gpu_slices";
      break;
    case sycl::aspect::ext_intel_gpu_subslices_per_slice:
      out << "ext_intel_gpu_subslices_per_slice";
      break;
    case sycl::aspect::ext_intel_gpu_eu_count_per_subslice:
      out << "ext_intel_gpu_eu_count_per_subslice";
      break;
    case sycl::aspect::ext_intel_max_mem_bandwidth:
      out << "ext_intel_max_mem_bandwidth";
      break;
    case sycl::aspect::ext_intel_mem_channel:
      out << "ext_intel_mem_channel";
      break;
    case sycl::aspect::ext_intel_device_info_uuid:
      out << "ext_intel_device_info_uuid";
      break;
    case sycl::aspect::ext_intel_gpu_hw_threads_per_eu:
      out << "ext_intel_gpu_hw_threads_per_eu";
      break;
    case sycl::aspect::ext_intel_free_memory:
      out << "ext_intel_free_memory";
      break;
    case sycl::aspect::ext_intel_device_id:
      out << "ext_intel_device_id";
      break;
    case sycl::aspect::ext_intel_memory_clock_rate:
      out << "ext_intel_memory_clock_rate";
      break;
    case sycl::aspect::ext_intel_memory_bus_width:
      out << "ext_intel_memory_bus_width";
      break;
#if __LIBSYCL_MAJOR_VERSION > 6
    case sycl::aspect::ext_intel_legacy_image:
      out << "ext_intel_legacy_image";
      break;
#endif
#endif
#if defined(SYCL_IMPLEMENTATION_ONEAPI)
    case sycl::aspect::ext_oneapi_srgb:
      out << "ext_oneapi_srgb";
      break;
    case sycl::aspect::ext_oneapi_native_assert:
      out << "ext_oneapi_native_assert";
      break;
    case sycl::aspect::ext_oneapi_cuda_async_barrier:
      out << "ext_oneapi_cuda_async_barrier";
      break;
    case sycl::aspect::ext_oneapi_bfloat16_math_functions:
      out << "ext_oneapi_bfloat16_math_functions";
      break;
#endif
    default:
      out << "<unknown-aspect: " << static_cast<std::size_t>(sycl_aspect) << ">";
      break;
  }
  return out;
}

int main(int argc, char* argv[]) {
  bool list_mode = false;

  if (argc > 1 && std::string(argv[1]).compare("-l") == 0) {
    list_mode = true;
  }

  std::vector<sycl::platform> platforms =
    sycl::platform::get_platforms();

  if (list_mode) {
    for (size_t pl_id = 0; pl_id < platforms.size(); pl_id++) {
      std::string name =
        platforms[pl_id].get_info<sycl::info::platform::name>();
      std::cout << "Platform #" << pl_id << ": " << name << std::endl;

      std::vector<sycl::device> devices =
        platforms[pl_id].get_devices(sycl::info::device_type::all);

      for (size_t device_id = 0; device_id < devices.size(); device_id++) {
        std::string name =
          devices[device_id].get_info<sycl::info::device::name>();
        std::cout << " `-- Device #" << device_id << ": " << name << "\n";
      }
    }
    std::cout << std::endl;
  } else {
    std::cout << std::setw(TEXT_WIDTH) << std::left <<
      TAB + "Number of  platforms " << platforms.size() << std::endl;

    for (auto platform : platforms) {
      std::string name =
        platform.get_info<sycl::info::platform::name>();
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        TAB + "Platform Name " << name << std::endl;

      std::string vendor =
        platform.get_info<sycl::info::platform::vendor>();
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        TAB + "Platform Vendor " << vendor << std::endl;

      std::string profile =
        platform.get_info<sycl::info::platform::profile>();
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        TAB + "Platform Profile " << profile << std::endl;
    }

    for (auto platform : platforms) {
      std::string name =
        platform.get_info<sycl::info::platform::name>();
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        TAB + "Platform Name " << name << std::endl;

      std::vector<sycl::device> devices =
        platform.get_devices(sycl::info::device_type::all);
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        "Number of devices " << devices.size() << std::endl;

      if (devices.size() == 0) {
        std::cout << std::endl;
      }

      for (auto device : devices) {
        std::string name =
          device.get_info<sycl::info::device::name>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Name " << name << std::endl;

        std::string vendor =
          device.get_info<sycl::info::device::vendor>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Vendor "<< vendor << std::endl;

        cl_uint vendor_id = device.get_info<sycl::info::device::vendor_id>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device vendor ID " << "0x" <<
          std::hex << vendor_id << std::dec << std::endl;

        std::string device_version =
          device.get_info<sycl::info::device::version>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Version " << device_version << std::endl;

        std::string driver_version =
          device.get_info<sycl::info::device::driver_version>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Driver Version " << driver_version << std::endl;

        std::string version =
          device.get_info<sycl::info::device::version>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device SYCL Vesrion " << version << std::endl;

        sycl::info::device_type device_type =
          device.get_info<sycl::info::device::device_type>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device type ";
        switch (device_type) {
          case sycl::info::device_type::gpu:
            std::cout << "GPU";
            break;
          case sycl::info::device_type::cpu:
            std::cout << "CPU";
            break;
          case sycl::info::device_type::host:
            std::cout << "HOST";
            break;
          case sycl::info::device_type::accelerator:
            std::cout << "ACCELERATOR";
            break;
          default:
            std::cout << "OTHER";
        }
        std::cout << std::endl;

        bool is_avilable =
          device.get_info<sycl::info::device::is_available>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Available " <<
          (is_avilable ? "Yes" : "No") << std::endl;

        bool is_compiler_avilable =
          device.has(sycl::aspect::online_compiler);

        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Compiler Available " <<
          (is_compiler_avilable ? "Yes" : "No") << std::endl;

        bool is_linker_avilable =
          device.has(sycl::aspect::online_linker);
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Linker Available " <<
          (is_linker_avilable ? "Yes" : "No") << std::endl;

        cl_uint max_compute_units =
          device.get_info<sycl::info::device::max_compute_units>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Max compute units " << max_compute_units << std::endl;

        if (device_type != sycl::info::device_type::host) {
          cl_uint maxClockFrequency =
            device.get_info<sycl::info::device::max_clock_frequency>();
          std::cout << std::setw(TEXT_WIDTH) << std::left <<
            TAB + "Max clock frequency " <<
            maxClockFrequency << "MHz" << std::endl;
        }

        cl_uint max_work_item_dimensions =
          device.get_info<sycl::info::device::max_work_item_dimensions>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Max work item dimensions " <<
          max_work_item_dimensions << std::endl;

        sycl::id<3> sizes =
          device.get_info<sycl::info::device::max_work_item_sizes<3>>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Max work item sizes " <<
          sizes[0] << " x " << sizes[1] << " x " << sizes[2] << std::endl;

        size_t max_work_group_size =
          device.get_info<sycl::info::device::max_work_group_size>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Max work group size " <<
          max_work_group_size << std::endl;

        cl_ulong global_mem_size =
          device.get_info<sycl::info::device::global_mem_size>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Global memory size " << global_mem_size <<
          " (" << ConvertBytesToString(global_mem_size) << ")" << std::endl;

        sycl::info::global_mem_cache_type global_mem_cache_type =
          device.get_info<sycl::info::device::global_mem_cache_type>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Global Memory cache type ";
        switch (global_mem_cache_type) {
          case sycl::info::global_mem_cache_type::none:
            std::cout << "None" << std::endl;
            break;
          case sycl::info::global_mem_cache_type::read_only:
            std::cout << "Read Only" << std::endl;
            break;
          case sycl::info::global_mem_cache_type::read_write:
            std::cout << "Read/Write" << std::endl;
            break;
          default:
            std::cout << "Other" << std::endl;
        }

        // TODO(matthew.schilling@intel.com): Deprecated, however, I do not
        // know a drop-in replacement.
        // https://registry.khronos.org/SYCL/specs/sycl-2020/html/sycl-2020.html#_device_information_descriptors
        bool preferred_interop_user_sync =
          device.get_info<sycl::info::device::preferred_interop_user_sync>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Prefer user sync for interop " <<
          (is_compiler_avilable ? "Yes" : "No") << std::endl;

        size_t profiling_timer_resolution =
          device.get_info<sycl::info::device::profiling_timer_resolution>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Profiling timer resolution " <<
          profiling_timer_resolution << "ns" << std::endl;

        // TODO(matthew.schilling@intel.com): Deprecated, however, I do not
        // know a drop-in replacement.
        // https://registry.khronos.org/SYCL/specs/sycl-2020/html/sycl-2020.html#_device_information_descriptors
        size_t printf_buffer_size =
          device.get_info<sycl::info::device::printf_buffer_size>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "printf() buffer size" << printf_buffer_size << " (" <<
          ConvertBytesToString(printf_buffer_size) << ")" << std::endl;

        auto built_in_kernels =
          device.get_info<sycl::info::device::built_in_kernel_ids>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Built-in kernels ";
        for (size_t i = 0; i < built_in_kernels.size(); ++i) {
          std::cout << built_in_kernels[i].get_name() << " ";
        }
        std::cout << std::endl;

        // TODO(matthew.schilling@intel.com): Deprecated, however, the
        // suggested replacement is not 1-for-1. I added the implementation
        // using aspects below. Keeping this for now.
        // https://registry.khronos.org/SYCL/specs/sycl-2020/html/sycl-2020.html#_device_information_descriptors
        std::vector<std::string> device_extensions =
          device.get_info<sycl::info::device::extensions>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Extensions ";
        for (const auto& device_extension : device_extensions) {
          std::cout << device_extension << " ";
        }
        std::cout << std::endl;

        auto device_aspects =
          device.get_info<sycl::info::device::aspects>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Aspects ";
        for (const auto& device_aspect : device_aspects) {
          std::cout << device_aspect << " ";
        }
        std::cout << std::endl << std::endl;
      }
    }
  }

  return 0;
}

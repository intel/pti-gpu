#include <iostream>
#include <iomanip>
#include <string>

#include <CL/sycl.hpp>

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


int main(int argc, char* argv[]) {
  bool list_mode = false;

  if (argc > 1 && std::string(argv[1]).compare("-l") == 0) {
    list_mode = true;
  }

  sycl::vector_class<sycl::platform> platforms =
    sycl::platform::get_platforms();

  if (list_mode) {
    for (size_t pl_id = 0; pl_id < platforms.size(); pl_id++) {
      std::string name =
        platforms[pl_id].get_info<sycl::info::platform::name>();
      std::cout << "Platform #" << pl_id << ": " << name << std::endl;

      sycl::vector_class<sycl::device> devices =
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

      sycl::vector_class<std::string> extensions =
        platform.get_info<sycl::info::platform::extensions>();
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        TAB + "Platform Extensions ";
      for (auto extenton : extensions) {
        std::cout << extenton << " ";
      }
      std::cout << std::endl << std::endl;
    }

    for (auto platform : platforms) {
      std::string name =
        platform.get_info<sycl::info::platform::name>();
      std::cout << std::setw(TEXT_WIDTH) << std::left <<
        TAB + "Platform Name " << name << std::endl;

      sycl::vector_class<sycl::device> devices =
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
          device.get_info<sycl::info::device::is_compiler_available>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Compiler Available " <<
          (is_compiler_avilable ? "Yes" : "No") << std::endl;

        bool is_linker_avilable =
          device.get_info<sycl::info::device::is_linker_available>();
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
          device.get_info<sycl::info::device::max_work_item_sizes>();
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

        size_t printf_buffer_size =
          device.get_info<sycl::info::device::printf_buffer_size>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "printf() buffer size" << printf_buffer_size << " (" <<
          ConvertBytesToString(printf_buffer_size) << ")" << std::endl;

        sycl::vector_class<std::string> built_in_kernels =
          device.get_info<sycl::info::device::built_in_kernels>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Built-in kernels ";
        for (auto kernel : built_in_kernels) {
          std::cout << kernel << " ";
        }
        std::cout << std::endl;

        sycl::vector_class<std::string> device_extensions =
          device.get_info<sycl::info::device::extensions>();
        std::cout << std::setw(TEXT_WIDTH) << std::left <<
          TAB + "Device Extensions ";
        for (auto device_extension : device_extensions) {
          std::cout << device_extension << " ";
        }
        std::cout << std::endl << std::endl;
      }
    }
  }

  return 0;
}
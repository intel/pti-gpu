/*
 Copyright 2016-2020 Intel Corporation

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#pragma once

#include <algorithm>
#if __has_include(<sycl/sycl.hpp>)
#include <sycl/sycl.hpp>
#else
#error "Unsupported compiler"
#endif
#if __has_include(<CL/sycl/property_list.hpp>)
#include <CL/sycl/property_list.hpp>
#elif __has_include(<sycl/property_list.hpp>)
#include <sycl/property_list.hpp>
#else
#error "Unsupported compiler"
#endif
#include <iostream>
#include <map>
#include <mpi.h>
#include <numeric>
#include <set>
#include <string>
#include <numeric>

#include "base.hpp"
#include "base_utils.hpp"
#include "oneapi/ccl.hpp"

#if defined(__INTEL_LLVM_COMPILER)
#if (__INTEL_LLVM_COMPILER < 20230000)
#define CCL_USE_SYCL121_API 1
#else // (__INTEL_LLVM_COMPILER < 20230000)
#define CCL_USE_SYCL121_API 0
#endif // (__INTEL_LLVM_COMPILER < 20230000)
#elif defined(__LIBSYCL_MAJOR_VERSION)
#if (__LIBSYCL_MAJOR_VERSION < 6)
#define CCL_USE_SYCL121_API 1
#else // (__LIBSYCL_MAJOR_VERSION < 6)
#define CCL_USE_SYCL121_API 0
#endif // (__LIBSYCL_MAJOR_VERSION < 6)
#else // __INTEL_LLVM_COMPILER || __LIBSYCL_MAJOR_VERSION
#error "Unsupported compiler"
#endif

#ifdef SYCL_LANGUAGE_VERSION
#define ICPX_VERSION __clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__
#else // SYCL_LANGUAGE_VERSION
#define ICPX_VERSION 0
#endif // SYCL_LANGUAGE_VERSION

namespace ccl {
#if CCL_USE_SYCL121_API
const auto cpu_selector_v = ::sycl::cpu_selector{};
const auto gpu_selector_v = ::sycl::gpu_selector{};
const auto default_selector_v = ::sycl::default_selector{};
#else // CCL_USE_SYCL121_API
inline const auto& cpu_selector_v = ::sycl::cpu_selector_v;
inline const auto& gpu_selector_v = ::sycl::gpu_selector_v;
inline const auto& default_selector_v = ::sycl::default_selector_v;
#endif // CCL_USE_SYCL121_API
} // namespace ccl

/* help functions for sycl-specific base implementation */
inline bool has_gpu() {
    std::vector<sycl::device> devices = sycl::device::get_devices();
    for (const auto& device : devices) {
        if (device.is_gpu()) {
            return true;
        }
    }
    return false;
}

inline bool has_accelerator() {
    std::vector<sycl::device> devices = sycl::device::get_devices();
    for (const auto& device : devices) {
        if (device.is_accelerator()) {
            return true;
        }
    }
    return false;
}

inline bool check_sycl_usm(sycl::queue& q, sycl::usm::alloc alloc_type) {
    bool ret = true;

    sycl::device d = q.get_device();

    if ((alloc_type == sycl::usm::alloc::host) && (d.is_gpu() || d.is_accelerator()))
        ret = false;

    if ((alloc_type == sycl::usm::alloc::device) && !(d.is_gpu() || d.is_accelerator()))
        ret = false;

    if (!ret) {
        std::cout << "incompatible device type and USM type\n";
    }

    return ret;
}

inline bool hide_platform_info() {
    int hide_info = false;
    char* hide_info_str = getenv("CCL_PLATFORM_INFO_HIDE");
    if (hide_info_str) {
        if (strcmp(hide_info_str, "0") == 0) {
            hide_info = false;
        }
        else if (strcmp(hide_info_str, "1") == 0) {
            hide_info = true;
        }
        else {
            throw std::runtime_error("invalid value for CCL_PLATFORM_INFO_HIDE: " +
                                     std::string(hide_info_str));
        }
    }
    return hide_info;
}

inline std::string get_preferred_gpu_platform_name() {
    std::string result;

    std::string filter = "level-zero";
    char* env = getenv("ONEAPI_DEVICE_SELECTOR");
    if (env) {
        if (std::strstr(env, "level_zero")) {
            filter = "level-zero";
        }
        else if (std::strstr(env, "opencl")) {
            filter = "opencl";
        }
        else {
            throw std::runtime_error("invalid device filter: " + std::string(env));
        }
    }

    auto plaform_list = sycl::platform::get_platforms();

    for (const auto& platform : plaform_list) {
        auto devices = platform.get_devices();
        auto gpu_dev = std::find_if(devices.begin(), devices.end(), [](const sycl::device& d) {
            return d.is_gpu();
        });

        if (gpu_dev == devices.end()) {
            // std::cout << "platform [" << platform_name
            //      << "] does not contain GPU devices, skipping\n";
            continue;
        }

        auto platform_name = platform.get_info<sycl::info::platform::name>();
        std::string platform_name_low_case;
        platform_name_low_case.resize(platform_name.size());

        std::transform(
            platform_name.begin(), platform_name.end(), platform_name_low_case.begin(), ::tolower);

        if (platform_name_low_case.find(filter) == std::string::npos) {
            // std::cout << "platform [" << platform_name
            //      << "] does not match with requested "
            //      << filter << ", skipping\n";
            continue;
        }

        result = platform_name;
    }

    if (result.empty())
        throw std::runtime_error("can not find preferred GPU platform");

    return result;
}

inline std::vector<sycl::device> create_sycl_gpu_devices(bool select_root_devices) {
    constexpr char prefix[] = "-- ";

    std::vector<sycl::device> result;
    auto plaform_list = sycl::platform::get_platforms();
    auto preferred_platform_name = get_preferred_gpu_platform_name();

    std::stringstream ss;
    std::stringstream ss_warn;

    for (const auto& platform : plaform_list) {
        auto platform_name = platform.get_info<sycl::info::platform::name>();
        if (platform_name.compare(preferred_platform_name) != 0) {
            continue;
        }

        auto device_list = platform.get_devices();
        for (const auto& device : device_list) {
            auto device_name = device.get_info<sycl::info::device::name>();

            if (!device.is_gpu()) {
                ss_warn << prefix << "device [" << device_name << "] is not GPU, skipping\n";
                continue;
            }

            if (select_root_devices) {
                result.push_back(device);
                continue;
            }

            auto part_props = device.get_info<sycl::info::device::partition_properties>();

            if (std::find(part_props.begin(),
                          part_props.end(),
                          sycl::info::partition_property::partition_by_affinity_domain) ==
                part_props.end()) {
                // ZE_FLAT_DEVICE_HIERARCHY=FLAT is by default now, meaning that
                // tile is a root device, the warning is extra in this case
                // ss_warn << prefix << "device [" << device_name
                //         << "] does not support partition by affinity domain"
                //         << ", use root device\n";
                result.push_back(device);
                continue;
            }

            auto part_affinity_domains =
                device.get_info<sycl::info::device::partition_affinity_domains>();

            if (std::find(part_affinity_domains.begin(),
                          part_affinity_domains.end(),
                          sycl::info::partition_affinity_domain::next_partitionable) ==
                part_affinity_domains.end()) {
                ss_warn << prefix << "device [" << device_name
                        << "] does not support next_partitionable affinity domain"
                        << ", use root device\n";
                result.push_back(device);
                continue;
            }

            auto sub_devices = device.create_sub_devices<
                sycl::info::partition_property::partition_by_affinity_domain>(
                sycl::info::partition_affinity_domain::next_partitionable);

            size_t sub_devices_max =
                device.template get_info<sycl::info::device::partition_max_sub_devices>();
            if (sub_devices.size() != sub_devices_max) {
                ss_warn << prefix << "device [" << device_name << "] expected " << sub_devices_max
                        << " sub-devices, but got " << sub_devices.size();
            }

            if (sub_devices.empty()) {
                ss_warn << prefix << "device [" << device_name << "] does not provide sub-devices"
                        << ", use root device\n";
                result.push_back(device);
                continue;
            }

            result.insert(result.end(), sub_devices.begin(), sub_devices.end());
        }
    }

    if (result.empty()) {
        throw std::runtime_error("no GPU devices found");
    }

    if (!hide_platform_info()) {
        ss << "preferred platform: " << preferred_platform_name << ", found: " << result.size()
           << " GPU device(s)\n";
    }
    ss << ss_warn.str();
    printf("%s", ss.str().c_str());

    return result;
}

inline std::vector<sycl::queue> create_sycl_queues(const std::string& device_type,
                                                   const std::vector<int>& ranks,
                                                   bool select_root_devices = false,
                                                   const sycl::property_list& queue_props = {}) {
    std::vector<sycl::device> devices;

    try {
        if (device_type.compare("gpu") == 0) {
            if (!has_gpu()) {
                throw std::runtime_error("GPU is requested but not available");
            }

            /* GPU type has special handling to cover multi-tile case */
            devices = create_sycl_gpu_devices(select_root_devices);
        }
        else {
            if (device_type.compare("cpu") == 0) {
                devices.push_back(sycl::device(ccl::cpu_selector_v));
            }
            else if (device_type.compare("default") == 0) {
                if (!has_accelerator()) {
                    devices.push_back(sycl::device(ccl::default_selector_v));
                }
                else {
                    devices.push_back(sycl::device(ccl::cpu_selector_v));
                    std::cout
                        << "Accelerator is the first in device list, but unavailable for multiprocessing "
                        << " cpu_selector has been created instead of default_selector.\n";
                }
            }
            else {
                throw std::runtime_error("Please provide device type: cpu | gpu | default");
            }
        }
    }
    catch (...) {
        throw std::runtime_error("No devices of requested type available");
    }

    if (devices.empty()) {
        throw std::runtime_error("No devices of requested type available");
    }

    int global_rank = 0, local_rank = 0;
    int global_size = 0, local_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &global_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &global_size);

    MPI_Comm local_comm;
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &local_comm);
    MPI_Comm_rank(local_comm, &local_rank);
    MPI_Comm_size(local_comm, &local_size);
    MPI_Comm_free(&local_comm);

    std::stringstream error_msg;

    if (local_rank > global_rank) {
        error_msg << "Local rank should be less or equal to global rank (local_rank: " << local_rank
                  << ", global_rank: " << global_rank << ")";
        throw std::runtime_error(error_msg.str());
    }

    if (local_size > global_size) {
        error_msg << "Local size should be less or equal to global size (local_size: " << local_size
                  << ", global_size: " << global_size << ")";
        throw std::runtime_error(error_msg.str());
    }

    if (ranks.size() != 1) {
        error_msg << "Unexpected number of device ranks: " << ranks.size();
        throw std::runtime_error(error_msg.str());
    }

    if (ranks[0] != global_rank) {
        error_msg << "Unexpected device rank: " << ranks[0] << ", expected: " << global_rank;
        throw std::runtime_error(error_msg.str());
    }

    // use local rank for device selection
    std::vector<int> local_ranks(1, local_rank);

    std::vector<sycl::device> rank_devices;
    for (size_t idx = 0; idx < local_ranks.size(); idx++) {
        rank_devices.push_back(devices[local_ranks[idx] % devices.size()]);
    }

    if (rank_devices.empty()) {
        throw std::runtime_error("No devices of requested type available for specified ranks");
    }

    sycl::context ctx;

    try {
        ctx = sycl::context(rank_devices);
    }
    catch (sycl::exception&) {
        size_t preferred_idx = (local_ranks.back() / local_ranks.size()) % devices.size();
        std::cout << "Can not create context from all rank devices of type: " << device_type
                  << ", create context from single device, idx " << preferred_idx << "\n";
        ctx = sycl::context(devices[preferred_idx]);
    }

    auto exception_handler = [&](sycl::exception_list elist) {
        for (std::exception_ptr const& e : elist) {
            try {
                rethrow_exception(e);
            }
            catch (std::exception const& e) {
                std::cout << "failure\n";
            }
        }
    };

    auto ctx_devices = ctx.get_devices();

    if (ctx_devices.empty()) {
        throw std::runtime_error("No devices of requested type available in context");
    }

    std::vector<sycl::queue> queues;

    if (!hide_platform_info()) {
        std::cout << "Created context from devices of type: " << device_type << "\n";
        std::cout << "Devices [" << ctx_devices.size() << "]:\n";
    }

    for (size_t idx = 0; idx < ctx_devices.size(); idx++) {
        if (!hide_platform_info()) {
            std::cout << "[" << idx << "]: ["
                      << ctx_devices[idx].get_info<sycl::info::device::name>() << "]\n";
        }
        queues.push_back(sycl::queue(ctx_devices[idx], exception_handler, queue_props));
    }

    return queues;
}

inline bool create_sycl_queue(const std::string& type,
                              int rank,
                              sycl::queue& q,
                              const sycl::property_list& queue_props = {}) {
    if (type == "gpu" || type == "cpu" || type == "host" || type == "default") {
        try {
            std::vector<int> ranks = { rank };
            q = create_sycl_queues(type, ranks, false, queue_props)[0];
            return true;
        }
        catch (std::exception& e) {
            std::cerr << e.what() << "\n";
            return false;
        }
    }
    else {
        std::cerr << "Unknown device type: " << type
                  << ", please provide: cpu | gpu | host | default\n";
        return false;
    }
}

inline bool create_sycl_queue(int argc,
                              char* argv[],
                              int rank,
                              sycl::queue& q,
                              const sycl::property_list& queue_props = {}) {
    return create_sycl_queue(((argc >= 2) ? argv[1] : "unknown"), rank, q, queue_props);
}

inline bool handle_exception(sycl::queue& q) {
    try {
        q.wait_and_throw();
    }
    catch (std::exception const& e) {
        std::cout << "Caught synchronous SYCL exception:\n" << e.what() << "\n";
        return false;
    }
    return true;
}

inline sycl::usm::alloc usm_alloc_type_from_string(const std::string& str) {
    const std::map<std::string, sycl::usm::alloc> names{ {
        { "host", sycl::usm::alloc::host },
        { "device", sycl::usm::alloc::device },
        { "shared", sycl::usm::alloc::shared },
    } };

    auto it = names.find(str);
    if (it == names.end()) {
        std::stringstream ss;
        ss << "Invalid USM type requested: " << str << "\nSupported types are:\n";
        for (const auto& v : names) {
            ss << v.first << ", ";
        }
        throw std::runtime_error(ss.str());
    }
    return it->second;
}

inline std::pair<sycl::usm::alloc, std::string> take_usm_type(const int argc, char* str_type) {
    std::map<sycl::usm::alloc, std::string> map_usm_type;
    auto usm_alloc_type = sycl::usm::alloc::shared;
    auto str_usm_alloc_type = "shared";
    if (argc > 1) {
        str_usm_alloc_type = str_type;
        usm_alloc_type = usm_alloc_type_from_string(str_usm_alloc_type);
    }

    return std::make_pair(usm_alloc_type, str_usm_alloc_type);
}

template <typename T>
struct buf_allocator {
    const size_t alignment = 64;

    buf_allocator(sycl::queue& q) : q(q) {}

    buf_allocator& operator=(const buf_allocator&) = delete;
    buf_allocator(const buf_allocator&) = delete;
    buf_allocator(buf_allocator&&) = default;

    ~buf_allocator() {
        for (auto& ptr : memory_storage) {
            sycl::free(ptr, q);
        }
    }

    T* allocate(size_t count, sycl::usm::alloc alloc_type) {
        T* ptr = nullptr;
        if (alloc_type == sycl::usm::alloc::host)
            ptr = sycl::aligned_alloc_host<T>(alignment, count, q);
        else if (alloc_type == sycl::usm::alloc::device)
            ptr = sycl::aligned_alloc_device<T>(alignment, count, q);
        else if (alloc_type == sycl::usm::alloc::shared)
            ptr = sycl::aligned_alloc_shared<T>(alignment, count, q);
        else
            throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + " - unexpected alloc_type");

        if (!ptr) {
            throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                     " - failed to allocate buffer");
        }

        auto it = memory_storage.find(ptr);
        if (it != memory_storage.end()) {
            throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                     " - allocator already owns this pointer");
        }
        memory_storage.insert(ptr);

        auto pointer_type = sycl::get_pointer_type(ptr, q.get_context());
        if (pointer_type != alloc_type)
            throw std::runtime_error(std::string(__PRETTY_FUNCTION__) + " - pointer_type " +
                                     std::to_string((int)pointer_type) +
                                     " doesn't match with requested " +
                                     std::to_string((int)alloc_type));

        return ptr;
    }

    void deallocate(T* ptr) {
        auto it = memory_storage.find(ptr);
        if (it == memory_storage.end()) {
            throw std::runtime_error(std::string(__PRETTY_FUNCTION__) +
                                     " - allocator doesn't own this pointer");
        }
        free(ptr, q);
        memory_storage.erase(it);
    }

    sycl::queue q;
    std::set<T*> memory_storage;
};

inline sycl::event submit_barrier(sycl::queue queue) {
#if ICPX_VERSION >= 140000
    return queue.ext_oneapi_submit_barrier();
#elif ICPX_VERSION < 140000
    return queue.submit_barrier();
#endif // ICPX_VERSION
}

enum class queue_type { in_order, out_of_order };

inline std::string convert_queue_type(queue_type val) {
    static std::unordered_map<queue_type, std::string> convert = {
        { queue_type::in_order, "in_order" }, { queue_type::out_of_order, "out_of_order" }
    };
    return convert[val];
}

struct test_args {
    int argc = 0;
    char** argv = nullptr;
    int rank = 0;
    queue_type queue = queue_type::in_order;
    size_t count = 0;
    bool group_api = false;

    static const size_t DEFAULT_COUNT = 10 * 1024 * 1024;

    test_args(int argc, char* argv[], int rank)
            : argc(argc),
              argv(argv),
              rank(rank),
              count(DEFAULT_COUNT) {
        parse();
        print();
    }

    void print() const {
        if (rank == 0) {
            std::cout << "queue_type: " << convert_queue_type(queue) << "\n";
            std::cout << "group_api: " << group_api << "\n";
            std::cout << "count: " << count << "\n";
        }
    }

    void print_help(const char* test) const {
        std::cout << "Usage: " << test << " [--queue_type <queue_type>] [--count <data_count>]\n";
        std::cout << "Options:\n";
        std::cout
            << " --queue_type <queue_type>   Choose in_order or out_of_order (default is in_order)\n";
        std::cout << " --count <data_count>  Specify the data count\n";
    }

    void parse() {
        std::unordered_map<std::string, std::string> args_map;
        if (argc > 1) {
            if ((std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0)) {
                if (rank == 0) {
                    print_help(argv[0]);
                }
                exit(0);
            }
        }

        // parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.substr(0, 2) == "--") {
                arg = arg.substr(2);
                if (i + 1 < argc && argv[i + 1][0] != '-') {
                    args_map[arg] = argv[i + 1];
                    ++i;
                }
                else {
                    args_map[arg] = "";
                }
            }
        }

        if (args_map.find("queue_type") != args_map.end()) {
            std::string type_str = args_map["queue_type"];
            if (type_str == "in_order") {
                queue = queue_type::in_order;
            }
            else if (type_str == "out_of_order") {
                queue = queue_type::out_of_order;
            }
            else {
                std::cerr << "Invalid queue_type: " << type_str << std::endl;
                exit(1);
            }
        }

        if (args_map.find("group_api") != args_map.end()) {
            std::string type_str = args_map["group_api"];
            if (type_str == "1") {
                group_api = true;
            }
            else if (type_str == "0") {
                group_api = false;
            }
            else {
                std::cerr << "Invalid group_api argument: " << type_str << std::endl;
                exit(1);
            }
        }

        if (args_map.find("count") != args_map.end()) {
            std::string count_str = args_map["count"];
            count = std::stoi(count_str);
        }
    }
};

// returns true if the queue is successfully created, otherwise false
inline bool create_test_sycl_queue(const std::string& type,
                                   int rank,
                                   sycl::queue& q,
                                   test_args& args) {
    sycl::property_list props;

    if (args.queue == queue_type::in_order) {
        props = { sycl::property::queue::in_order{}, sycl::property::queue::enable_profiling{} };
    }
    else {
        props = { sycl::property::queue::enable_profiling{} };
    }

    return create_sycl_queue(type, rank, q, props);
}

inline bool check_example_args(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "usage: " << argv[0] << " <device_type> <alloc_type>\n";
        std::cout << "device_type: 'cpu' or 'gpu'\n";
        std::cout << "alloc_type: 'host', 'device', or 'shared'\n";
        std::cout << "example: " << argv[0] << " cpu shared\n";
        return false;
    }

    std::string device_type = argv[1];
    std::string alloc_type = argv[2];

    if (device_type != "cpu" && device_type != "gpu") {
        std::cout << "error: Invalid device '" << device_type << "'.\n";
        std::cout << "device_type must be either 'cpu' or 'gpu'.\n";
        return false;
    }

    if (alloc_type != "host" && alloc_type != "device" && alloc_type != "shared") {
        std::cout << "error: Invalid alloc_type '" << alloc_type << "'.\n";
        std::cout << "alloc_type must be either 'host', 'device', or 'shared'.\n";
        return false;
    }

    return true;
}

//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sycl/exception.hpp>
#include <sycl/sycl.hpp>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "graph_dotproduct_workload_info.h"
#include "graph_record_format.h"
#include "pti/pti_view.h"
#include "sycl_graph_workloads.h"
#include "utils/sycl_config_info.h"
#include "utils/test_helpers.h"

template <>
struct fmt::formatter<pti_view_kind> : fmt::ostream_formatter {};

namespace {
constexpr int kSkipTest = 77;
constexpr int kPassTest = EXIT_SUCCESS;
constexpr int kFailTest = EXIT_FAILURE;

using Workload = DotProductWorkload<float>;

template <typename T, typename... U>
inline bool CheckArgument(const T& arg, const U&... args) {
  return ((!std::strcmp(arg.c_str(), args)) || ...);
}

struct ProgramSettings {
  constexpr static std::size_t kDefaultIterations = 1ULL;
  std::size_t iterations = kDefaultIterations;
  bool verbose = false;
};

ProgramSettings ParseArguments(const std::vector<std::string>& args) {
  ProgramSettings settings{};

  try {
    for (std::size_t arg_idx = 0; arg_idx < std::size(args); ++arg_idx) {
      if (CheckArgument(args[arg_idx], "-i", "--iterations")) {
        settings.iterations = std::stoull(args.at(++arg_idx));
      } else if (CheckArgument(args[arg_idx], "-v", "--verbose")) {
        settings.verbose = true;
      } else {
        throw std::invalid_argument(fmt::format("{}", args[arg_idx]));
      }
    }
  } catch (const std::out_of_range&) {
    throw std::invalid_argument("Missing argument for option and/or invalid argument size");
  }

  return settings;
}

class PtiCollector {
 public:
  using PtiViewBuffer = pti::test::utils::PtiViewBuffer;
  using CallStats = std::pair<uint64_t, std::size_t>;  // ns, count

  struct Records {
    void Reset() {
      kernel_records.clear();
      overhead_records.clear();
      const std::lock_guard<std::mutex> lock(buffers_mtx);
      buffers.clear();
    }

    mutable std::mutex buffers_mtx;
    std::unordered_map<unsigned char*, PtiViewBuffer> buffers;
    std::vector<const pti_view_record_kernel_v2*> kernel_records;
    std::vector<const pti_view_record_overhead*> overhead_records;
  };

  PtiCollector() {
    record_storage_.Reset();
    if (ptiViewSetCallbacks(ProvideBuffer, MarkBuffer) != PTI_SUCCESS) {
      throw std::runtime_error("Failed to set PTI view callbacks");
    }
  }

  PtiCollector(const PtiCollector&) = delete;
  PtiCollector& operator=(const PtiCollector&) = delete;
  PtiCollector(PtiCollector&&) = delete;
  PtiCollector& operator=(PtiCollector&&) = delete;
  ~PtiCollector() = default;

  static void ProvideBuffer(unsigned char** buf, size_t* buf_size) {
    auto buffer = PtiViewBuffer(kRequestedBufferSize);
    if (!buffer.Valid()) {
      fmt::print(stderr, "[FATAL] Unable to allocate buffer for PTI tracing\n");
      std::abort();
    }
    *buf = buffer.data();
    *buf_size = buffer.size();

    const std::lock_guard<std::mutex> lock(record_storage_.buffers_mtx);
    record_storage_.buffers[*buf] = std::move(buffer);
  }

  static void MarkBuffer(unsigned char* buf, size_t /*buf_size*/, size_t used_bytes) {
    const std::lock_guard<std::mutex> lock(record_storage_.buffers_mtx);
    if (auto it = record_storage_.buffers.find(buf); it != record_storage_.buffers.end()) {
      it->second.SetUsedBytes(used_bytes);
    }
  }

  static void HandleView(pti_view_record_base* view) {
    switch (view->_view_kind) {
      case PTI_VIEW_DEVICE_GPU_KERNEL:
        record_storage_.kernel_records.push_back(
            reinterpret_cast<const pti_view_record_kernel_v2*>(view));
        break;
      case PTI_VIEW_COLLECTION_OVERHEAD:
        record_storage_.overhead_records.push_back(
            reinterpret_cast<const pti_view_record_overhead*>(view));
        break;
      default:
        break;
    }
  }

  static void ParseBuffer(unsigned char* buf, size_t used_bytes) {
    pti_view_record_base* record = nullptr;
    while (true) {
      auto status = ptiViewGetNextRecord(buf, used_bytes, &record);
      if (status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (status != pti_result::PTI_SUCCESS) {
        fmt::print(stderr, "[FATAL] Failed to parse PTI view record: {}\n",
                   static_cast<std::size_t>(status));
        std::abort();
        break;
      }
      HandleView(record);
    }
  }

  static void ParseAllBuffers() {
    const std::lock_guard<std::mutex> lock(record_storage_.buffers_mtx);
    for (auto& [_, buffer] : record_storage_.buffers) {
      ParseBuffer(buffer.data(), buffer.UsedBytes());
    }
  }

  template <typename... E>
  void EnableViews(E... view_kinds) {
    (
        [&] {
          if (ptiViewEnable(view_kinds) != PTI_SUCCESS) {
            throw std::runtime_error(fmt::format("Failed to enable PTI view: {}", view_kinds));
          }
        }(),
        ...);
    (enabled_views_.push_back(view_kinds), ...);
  }

  void FinalizeCollection() {
    for (auto view_kind : enabled_views_) {
      if (ptiViewDisable(view_kind) != PTI_SUCCESS) {
        throw std::runtime_error(fmt::format("Failed to disable PTI view: {}", view_kind));
      }
    }
    if (ptiFlushAllViews() != PTI_SUCCESS) {
      throw std::runtime_error("Failed to flush PTI views");
    }
    ParseAllBuffers();
    overhead_kind_map_.reserve(record_storage_.overhead_records.size());
    for (const auto* record : record_storage_.overhead_records) {
      overhead_kind_map_[record->_api_id].first += record->_overhead_duration_ns;
      overhead_kind_map_[record->_api_id].second += 1;
    }
  }

  void PrintRecordStats(std::size_t iterations, std::chrono::duration<double> duration) {
    if (record_storage_.kernel_records.empty() || record_storage_.overhead_records.empty()) {
      throw std::runtime_error("No kernel or overhead records were collected");
    }
    fmt::print("  Graph Executions: {}, Graph Executions Per Second: {}\n", iterations,
               static_cast<double>(iterations) / duration.count());
    fmt::print("  Kernel Records: {}, Kernels Per Second: {}\n",
               record_storage_.kernel_records.size(),
               static_cast<double>(record_storage_.kernel_records.size()) / duration.count());
    fmt::print("  Overhead Records: {}\n", record_storage_.overhead_records.size());

    // Print call statistics for each overhead API record collected and sort by duration of the
    // call.
    std::vector<std::pair<uint32_t, CallStats>> sorted_overhead_records;
    sorted_overhead_records.reserve(overhead_kind_map_.size());
    for (const auto& [api_id, stats] : overhead_kind_map_) {
      sorted_overhead_records.emplace_back(api_id, stats);
    }

    std::sort(sorted_overhead_records.begin(), sorted_overhead_records.end(),
              [](const auto& first_rec_info, const auto& second_rec_info) {
                return first_rec_info.second.first > second_rec_info.second.first;
              });

    for (const auto& [api_id, total_overhead] : sorted_overhead_records) {
      const char* api_name = nullptr;
      auto res = ptiViewGetApiIdName(PTI_API_GROUP_LEVELZERO, api_id, &api_name);
      fmt::print(
          "  API: {}, # of Calls {}, Total Overhead (ns): {}, Percentage of Total workload "
          "duration: {}%\n",
          res == PTI_SUCCESS ? api_name : std::to_string(static_cast<std::size_t>(api_id)),
          total_overhead.second, total_overhead.first,
          (std::chrono::duration_cast<std::chrono::duration<double>>(
               std::chrono::nanoseconds(total_overhead.first))
               .count() /
           duration.count()) *
              100.0);
    }
  }

  static void PrintKernelRecords() {
    fmt::print("  Kernel Records:\n");
    for (const auto* record : record_storage_.kernel_records) {
      fmt::print("{}", FormatRecord(record));
    }
  }

  void ValidateKernelRecords(std::size_t iterations) {
    const auto expected_kernels = iterations * Workload::kDefaultKernelNumber;
    const auto found_kernels = std::size(record_storage_.kernel_records);
    if (found_kernels != expected_kernels) {
      fmt::print(stderr, "[FATAL] Expected {} kernel records, got {}\n", expected_kernels,
                 found_kernels);
      throw std::runtime_error("Kernel record count mismatch");
    }
    if (overhead_kind_map_.at(zeEventQueryStatus_id).second >= found_kernels) {
      throw std::runtime_error(
          "Too many zeEventQueryStatus calls per graph execution, expected less than 1 per kernel");
    }
  }

 private:
  inline static Records record_storage_{};
  constexpr static std::size_t kRequestedBufferSize = 1024 * 1024;
  std::vector<pti_view_kind> enabled_views_;
  std::unordered_map<uint32_t, CallStats> overhead_kind_map_;
};

[[maybe_unused]] double DotProductRelativeError(Workload::DefaultResultDataType result,
                                                std::size_t iterations) {
  const auto expected = static_cast<double>(Workload::Result()) * static_cast<double>(iterations);
  if (expected == 0.0) {
    throw std::invalid_argument("Dot product cannot be 0.");
  }

  return (std::abs(static_cast<double>(result) - expected) / std::abs(expected)) * 100.0;
}
}  // namespace
int main(int argc, char* argv[]) {
  try {
    [[maybe_unused]] auto settings =
        ParseArguments(std::vector<std::string>(argv + 1, argv + argc));  // NOLINT

#if defined(PTI_TEST_NATIVE_GRAPH_RECORDING_API_AVAILABLE)
    auto graph_selector = [](const sycl::device& dev) {
      if (dev.has(sycl::aspect::ext_oneapi_graph)) {
        return 2;
      }
      if (dev.has(sycl::aspect::ext_oneapi_limited_graph)) {
        return 1;
      }

      return -1;
    };

    auto sycl_queue = sycl::queue{graph_selector, {sycl::property::queue::in_order()}};

    auto [dot_product, x_vector, y_vector, z_vector] =
        CreateUsmDotProductVectors<Workload::DefaultVectorDataType>(sycl_queue,
                                                                    Workload::kDefaultVectorSize);
    *dot_product = static_cast<Workload::DefaultResultDataType>(0);

    auto graph =
        CreateNativeUsmDotProductGraph(sycl_queue, Workload::kDefaultVectorSize, dot_product.get(),
                                       x_vector.get(), y_vector.get(), z_vector.get());

    const auto exec = graph.finalize();

    PtiCollector collector{};
    collector.EnableViews(PTI_VIEW_DEVICE_GPU_KERNEL, PTI_VIEW_COLLECTION_OVERHEAD);

    auto start_graph_execs = std::chrono::high_resolution_clock::now();
    for (std::size_t i = 0; i < settings.iterations; ++i) {
      sycl_queue.ext_oneapi_graph(exec).wait_and_throw();
    }
    auto end_graph_execs = std::chrono::high_resolution_clock::now();

    collector.FinalizeCollection();

    const auto total_duration = std::chrono::duration<double>(end_graph_execs - start_graph_execs);

    fmt::print("Completed {} iterations of graph execution(s) in {} seconds\n", settings.iterations,
               total_duration.count());

    collector.PrintRecordStats(settings.iterations, total_duration);
    if (settings.verbose) {
      PtiCollector::PrintKernelRecords();
    }
    collector.ValidateKernelRecords(settings.iterations);
    constexpr static double kMaxErrorPercentage = 5.0;
    const auto result_valid = DotProductRelativeError(*dot_product, settings.iterations);
    if (result_valid > kMaxErrorPercentage) {
      throw std::runtime_error(fmt::format("Dot product validation failed: {}% > {}%", result_valid,
                                           kMaxErrorPercentage));
    }
#else
    fmt::print(stdout, "[SKIP] Test skipped due to unsupported native graph recording API\n");
    return kSkipTest;
#endif  // PTI_TEST_NATIVE_GRAPH_RECORDING_API_AVAILABLE

  } catch (const sycl::exception& e) {
    if (e.code() == sycl::make_error_code(sycl::errc::feature_not_supported) ||
        e.code() == sycl::make_error_code(sycl::errc::invalid)) {
      fmt::print(stdout, "[SKIP] Test skipped due to unsupported feature: {}\n", e.what());
      return kSkipTest;
    }
    fmt::print(stderr, "[FATAL] SYCL Exception: {}. Error code {}\n", e.what(),
               static_cast<std::size_t>(e.code().value()));
    return kFailTest;
  } catch (const std::invalid_argument& e) {
    fmt::print(stderr, "Invalid Argument: {}\n", e.what());
    return kFailTest;
  } catch (const std::exception& e) {
    fmt::print(stderr, "[FATAL] Error: {}\n", e.what());
    return kFailTest;
  } catch (...) {
    fmt::print(stderr, "[FATAL] Unknown Error\n");
    return kFailTest;
  }
  return kPassTest;
}

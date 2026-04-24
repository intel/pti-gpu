#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <algorithm>
#include <cstring>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"
#include "utils/pti_record_collection_fixture.h"
#include "utils/sycl_usm_helper.h"

namespace {
constexpr uint64_t kMaxQueueId = static_cast<uint64_t>(PTI_INVALID_QUEUE_ID);

constexpr const char* const kSubstringMemoryTypeDevice = "Device";
constexpr const char* const kSubstringMemoryTypeShared = "Shared";
constexpr const char* const kSubstringMemoryTypeHost = "Host";
constexpr const char* const kSubstringMemoryTypeDriverAllocated = "Driver";
constexpr const char* const kSubstringMemoryTypeUserAllocated = "User";

bool p2p_d2d_record = false;
bool p2p_d2s_record = false;
bool p2p_s2d_record = false;
bool p2p_s2s_record = false;
bool memfill_m2s = false;
bool memfill_m2d = false;
bool sycl_memfill_seen = false;
bool sycl_memcpy_seen = false;
bool sycl_host_alloc_seen = false;
bool sycl_device_alloc_seen = false;
bool sycl_shared_alloc_seen = false;
bool uuid_non_unique = false;
bool memfill_uuid_zero = false;
bool memcopy_type_valid = false;
bool memsrc_type_valid = false;
bool memdst_type_valid = false;
bool non_p2p_d2d_exists = false;
bool atleast_2_devices = false;
bool p2p_device_access = false;
bool memcopy_type_stringified = false;
bool memcopy_type_p2p_stringified = false;
bool memory_src_type_stringified = false;
bool memory_src_type_p2p_stringified = false;
bool memory_dst_type_stringified = false;
bool memory_dst_type_p2p_stringified = false;
bool queue_id_memp2p_records = false;
}  // namespace

void StartTracing() {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
}

void StopTracing() {
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_RUNTIME_API), pti_result::PTI_SUCCESS);
}

static void BufferRequested(unsigned char** buf, size_t* buf_size) {
  *buf_size = sizeof(pti_view_record_kernel);
  void* ptr = ::operator new(*buf_size);
  ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
  *buf = static_cast<unsigned char*>(ptr);
}

static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
  if (!buf || !used_bytes || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    ::operator delete(buf);
    return;
  }

  pti_view_record_base* ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      break;
    }
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      break;
    }
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_INVALID: {
        std::cout << "Found Invalid Record" << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        [[maybe_unused]] auto* record =
            reinterpret_cast<pti_view_record_external_correlation*>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        [[maybe_unused]] auto* record = reinterpret_cast<pti_view_record_overhead*>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        auto* rec = reinterpret_cast<pti_view_record_memory_copy*>(ptr);
        std::string memcpy_name = rec->_name;
        if (memcpy_name.find("D2D)") != std::string::npos) {
          non_p2p_d2d_exists = true;
          if (rec->_memcpy_type == pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2D) {
            memcopy_type_valid = true;
            memcopy_type_stringified =
                (std::strcmp(ptiViewMemcpyTypeToString(rec->_memcpy_type), "D2D") == 0);
          }
          if (rec->_mem_src == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE) {
            memsrc_type_valid = true;
            memory_src_type_stringified = (std::strcmp(ptiViewMemoryTypeToString(rec->_mem_src),
                                                       kSubstringMemoryTypeDevice) == 0);
          }
          if (rec->_mem_dst == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE) {
            memdst_type_valid = true;
            memory_dst_type_stringified = (std::strcmp(ptiViewMemoryTypeToString(rec->_mem_dst),
                                                       kSubstringMemoryTypeDevice) == 0);
          }
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P: {
        auto* rec = reinterpret_cast<pti_view_record_memory_copy_p2p*>(ptr);
        std::string memcpy_name = rec->_name;
        if (memcpy_name.find("D2D - P2P") != std::string::npos) {
          p2p_d2d_record = true;
        }
        if (memcpy_name.find("D2S - P2P") != std::string::npos) {
          p2p_d2s_record = true;
        }
        if (memcpy_name.find("S2D - P2P") != std::string::npos) {
          p2p_s2d_record = true;
        }
        if (memcpy_name.find("S2S - P2P") != std::string::npos) {
          p2p_s2s_record = true;
        }
        if (rec->_sycl_queue_id != kMaxQueueId) {
          queue_id_memp2p_records = true;
        }
        if (std::equal(std::begin(rec->_src_uuid), std::end(rec->_src_uuid),
                       std::begin(rec->_dst_uuid))) {
          uuid_non_unique = true;
        }
        if (p2p_d2s_record) {
          if (rec->_memcpy_type == pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2S) {
            memcopy_type_valid = true;
            memcopy_type_p2p_stringified =
                (std::strcmp(ptiViewMemcpyTypeToString(rec->_memcpy_type), "D2S") == 0);
          }
          if (rec->_mem_src == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE) {
            memsrc_type_valid = true;
            memory_src_type_p2p_stringified = (std::strcmp(ptiViewMemoryTypeToString(rec->_mem_src),
                                                           kSubstringMemoryTypeDevice) == 0);
          }
          if (rec->_mem_dst == pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED) {
            memdst_type_valid = true;
            memory_dst_type_p2p_stringified = (std::strcmp(ptiViewMemoryTypeToString(rec->_mem_dst),
                                                           kSubstringMemoryTypeShared) == 0);
          }
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        const auto* rec = reinterpret_cast<const pti_view_record_memory_fill*>(ptr);
        std::string tmp_str = rec->_name;
        if (std::all_of(std::begin(rec->_device_uuid), std::end(rec->_device_uuid),
                        [](auto raw_byte) { return raw_byte == 0; })) {
          memfill_uuid_zero = true;
        }
        memfill_m2s = memfill_m2s || ((rec->_mem_type == PTI_VIEW_MEMORY_TYPE_SHARED) &&
                                      (tmp_str.find("M2S") != std::string::npos));
        memfill_m2d = memfill_m2d || ((rec->_mem_type == PTI_VIEW_MEMORY_TYPE_DEVICE) &&
                                      (tmp_str.find("M2D") != std::string::npos));
        break;
      }
      case pti_view_kind::PTI_VIEW_RUNTIME_API: {
        auto* rec = reinterpret_cast<pti_view_record_api*>(ptr);
        const char* plain_function_name = nullptr;
        pti_result status = ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_SYCL, rec->_api_id,
                                                &plain_function_name);
        ASSERT_EQ(status, pti_result::PTI_SUCCESS);
        std::string function_name(plain_function_name);
        if ((function_name.find("EnqueueUSMFill") != std::string::npos) ||
            (function_name.find("USMEnqueueMemset") != std::string::npos)) {
          sycl_memfill_seen = true;
        } else if ((function_name.find("EnqueueUSMMemcpy") != std::string::npos) ||
                   (function_name.find("USMEnqueueMemcpy") != std::string::npos)) {
          sycl_memcpy_seen = true;
        } else if ((function_name.find("DeviceAlloc") != std::string::npos)) {
          sycl_device_alloc_seen = true;
        } else if ((function_name.find("SharedAlloc") != std::string::npos)) {
          sycl_shared_alloc_seen = true;
        } else if ((function_name.find("HostAlloc") != std::string::npos)) {
          sycl_host_alloc_seen = true;
        }
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        [[maybe_unused]] pti_view_record_kernel* rec =
            reinterpret_cast<pti_view_record_kernel*>(ptr);
        break;
      }
      default: {
        std::cerr << "This shouldn't happen" << '\n';
        break;
      }
    }
  }
  ::operator delete(buf);
}

void P2pTest() {
  StartTracing();

  sycl::platform platform(sycl::gpu_selector_v);

  std::vector<sycl::queue> gpu_queues;
  std::vector<float*> host_ptrs;
  std::vector<float*> gpu_device_ptrs;
  std::vector<float*> gpu_shared_ptrs;
  std::vector<sycl::context> gpu_context;
  size_t num_root_devices = 0;

  try {
    std::vector<sycl::device> gpu_devices = platform.get_devices();
    num_root_devices = gpu_devices.size();
    std::cout << "Number of Root Devices: " << num_root_devices << "\n";
    sycl::property_list prop{sycl::property::queue::in_order()};
    for (size_t i = 0; i < num_root_devices; i++) {
      gpu_context.push_back(sycl::context(gpu_devices[i]));
      gpu_queues.push_back(sycl::queue(gpu_context[i], gpu_devices[i], prop));
      gpu_device_ptrs.push_back(
          static_cast<float*>(malloc_device(num_root_devices * sizeof(float), gpu_queues[i])));
      gpu_shared_ptrs.push_back(
          static_cast<float*>(malloc_shared(num_root_devices * sizeof(float), gpu_queues[i])));
      host_ptrs.push_back(
          static_cast<float*>(malloc_host(num_root_devices * sizeof(float), gpu_queues[i])));
      // std::cout << "memset for root device#: " << i << std::endl;
      gpu_queues[i].memset(gpu_device_ptrs[i], 0, num_root_devices * sizeof(float)).wait();
      gpu_queues[i].memset(gpu_shared_ptrs[i], 0, num_root_devices * sizeof(float)).wait();
    }
  } catch (const sycl::exception& e) {
    FAIL() << "[ERROR] " << e.what();
  }
  if (num_root_devices > 0) {
    // This forces a non-p2p D2D record --- memcpy same device for testing purposes.
    gpu_queues[0]
        .memcpy(gpu_device_ptrs[0], gpu_device_ptrs[0], num_root_devices * sizeof(float))
        .wait();
  }
  if (num_root_devices > 1) {
    atleast_2_devices = true;
    ze_bool_t p2p_access = 0;
    uint32_t connected_dev1 = 0;
    uint32_t connected_dev2 = 1;
    for (uint32_t i = 0; i < num_root_devices; i++) {
      auto* src_dev_backend_handle =
          sycl::get_native<sycl::backend::ext_oneapi_level_zero>(gpu_queues[i].get_device());
      for (uint32_t j = 0; j < num_root_devices; j++) {
        if (i == j) {
          continue;
        }
        auto* dst_dev_backend_handle =
            sycl::get_native<sycl::backend::ext_oneapi_level_zero>(gpu_queues[j].get_device());

        if (src_dev_backend_handle && dst_dev_backend_handle &&
            (src_dev_backend_handle != dst_dev_backend_handle)) {
          auto status =
              zeDeviceCanAccessPeer(src_dev_backend_handle, dst_dev_backend_handle, &p2p_access);
          ASSERT_EQ(status, ZE_RESULT_SUCCESS);
          if (p2p_access) {
            p2p_device_access = true;
            connected_dev1 = i;
            connected_dev2 = j;
            break;
          }
        }
      }
      if (p2p_device_access) {
        std::cout << "Connected devices: " << connected_dev1 << ":" << connected_dev2 << std::endl;
        break;
      }
    }

    //"MemCopy D2D"
    gpu_queues[connected_dev2]
        .memcpy(gpu_device_ptrs[connected_dev2], gpu_device_ptrs[connected_dev1],
                num_root_devices * sizeof(float))
        .wait();
    //"MemCopy D2S"
    gpu_queues[connected_dev1]
        .memcpy(gpu_shared_ptrs[connected_dev1], gpu_device_ptrs[connected_dev2],
                num_root_devices * sizeof(float))
        .wait();
    //"MemCopy S2D"
    gpu_queues[connected_dev1]
        .memcpy(gpu_device_ptrs[connected_dev1], gpu_shared_ptrs[connected_dev2],
                num_root_devices * sizeof(float))
        .wait();
    //"MemCopy S2S"
    gpu_queues[connected_dev2]
        .memcpy(gpu_shared_ptrs[connected_dev2], gpu_shared_ptrs[connected_dev1],
                num_root_devices * sizeof(float))
        .wait();
  }
  StopTracing();
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  for (uint32_t i = 0; i < num_root_devices; i++) {
    sycl::free(gpu_device_ptrs[i], gpu_context[i]);
    sycl::free(gpu_shared_ptrs[i], gpu_context[i]);
    sycl::free(host_ptrs[i], gpu_context[i]);
  }
}

class MemoryOperationFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    p2p_d2d_record = false;
    p2p_d2s_record = false;
    p2p_s2d_record = false;
    p2p_s2s_record = false;
    memfill_m2s = false;
    memfill_m2d = false;
    sycl_memfill_seen = false;
    sycl_memcpy_seen = false;
    sycl_host_alloc_seen = false;
    sycl_device_alloc_seen = false;
    sycl_shared_alloc_seen = false;
    uuid_non_unique = false;
    memfill_uuid_zero = false;
    memcopy_type_valid = false;
    memsrc_type_valid = false;
    memdst_type_valid = false;
    non_p2p_d2d_exists = false;
    atleast_2_devices = false;
    p2p_device_access = false;
    memcopy_type_stringified = false;
    memcopy_type_p2p_stringified = false;
    memory_src_type_stringified = false;
    memory_src_type_p2p_stringified = false;
    memory_dst_type_stringified = false;
    memory_dst_type_p2p_stringified = false;
    queue_id_memp2p_records = false;
  }

  void TearDown() override {}
};

TEST_F(MemoryOperationFixtureTest, P2PMemoryCopyRecords) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  if (!atleast_2_devices)
    GTEST_SKIP() << "This system does not have atleast 2 Level0 gpu devices for P2P tests\n";
  if (!p2p_device_access)
    GTEST_SKIP() << "This system does not have a direct p2p connection between devices\n";
  ASSERT_EQ(p2p_d2d_record, true);
  ASSERT_EQ(p2p_d2s_record, true);
  ASSERT_EQ(p2p_s2d_record, true);
  ASSERT_EQ(p2p_s2s_record, true);
}

TEST_F(MemoryOperationFixtureTest, P2PuuidUniqueEachDevicePerP2P) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  if (!atleast_2_devices)
    GTEST_SKIP() << "This system does not have atleast 2 Level0 gpu devices for P2P tests\n";
  if (!p2p_device_access)
    GTEST_SKIP() << "This system does not have a direct p2p connection between devices\n";
  ASSERT_EQ(uuid_non_unique, false);
}

// Detect the nonp2p -- device to same device memcpy
TEST_F(MemoryOperationFixtureTest, NonP2PD2D) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(non_p2p_d2d_exists, true);
}

// Detect the nonp2p -- device to same device memcpy stringified types
TEST_F(MemoryOperationFixtureTest, NonP2PD2DStringified) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memcopy_type_stringified, true);
  ASSERT_EQ(memory_src_type_stringified, true);
  ASSERT_EQ(memory_dst_type_stringified, true);
}

TEST_F(MemoryOperationFixtureTest, MemFilluuidDeviceNonZero) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memfill_uuid_zero, false);
}

TEST_F(MemoryOperationFixtureTest, MemCopyTypeDevice) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memcopy_type_valid, true);
}

TEST_F(MemoryOperationFixtureTest, MemSrcTypeDevice) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memsrc_type_valid, true);
}

TEST_F(MemoryOperationFixtureTest, MemDstTypeDevice) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memdst_type_valid, true);
}

TEST_F(MemoryOperationFixtureTest, MemCopyTypeP2PDevice) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memcopy_type_valid, true);
}

TEST_F(MemoryOperationFixtureTest, MemSrcTypeShared) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memsrc_type_valid, true);
}

TEST_F(MemoryOperationFixtureTest, MemDstTypeShared) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memdst_type_valid, true);
}

TEST_F(MemoryOperationFixtureTest, MemFillDstTypeSharedPresent) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memfill_m2s, true);
}

TEST_F(MemoryOperationFixtureTest, MemFillDstTypeDevicePresent) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  ASSERT_EQ(memfill_m2d, true);
}

TEST_F(MemoryOperationFixtureTest, P2PD2DStringified) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  if (!atleast_2_devices)
    GTEST_SKIP() << "This system does not have atleast 2 Level0 gpu devices for P2P tests\n";
  ASSERT_EQ(memcopy_type_p2p_stringified, true);
  ASSERT_EQ(memory_src_type_p2p_stringified, true);
  ASSERT_EQ(memory_dst_type_p2p_stringified, true);
}

TEST_F(MemoryOperationFixtureTest, P2PQueueIdPresent) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  if (!atleast_2_devices)
    GTEST_SKIP() << "This system does not have atleast 2 Level0 gpu devices for P2P tests\n";
  ASSERT_EQ(queue_id_memp2p_records, true);
}

TEST_F(MemoryOperationFixtureTest, SyclRuntimeRecordsDetected) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  P2pTest();
  EXPECT_EQ(sycl_host_alloc_seen, true);
  EXPECT_EQ(sycl_device_alloc_seen, true);
  EXPECT_EQ(sycl_shared_alloc_seen, true);
  EXPECT_EQ(sycl_memfill_seen, true);
  EXPECT_EQ(sycl_memcpy_seen, true);
}

// TODO(PTI): Right now, each member of MemoryOperationFixtureTests runs the
// same P2pTest, which is way overkill for simple memory functional tests. This new fixture will run
// each scenario independently.
class MemoryOperationTest : public ::pti::test::utils::RecordCollectionFixture {
 protected:
  void SetUp() override {
    try {
      const sycl::property_list prop_list{
          sycl::property::queue::in_order(),
          sycl::ext::intel::property::queue::immediate_command_list()};
      queue_ = sycl::queue(sycl::gpu_selector_v, prop_list);
    } catch (const sycl::exception& e) {
      GTEST_SKIP() << "No SYCL device or queue available: " << e.what();
    } catch (...) {
      FAIL() << "Unknown exception during SYCL device or queue creation.";
    }
  }

  sycl::queue queue_;
};

TEST_F(MemoryOperationTest, SuccessfulMemFillDriverAllocated) {
  InitCollection();
  EnableViews(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  constexpr std::size_t kVecSize = 64;
  auto host_vector = pti::test::utils::CreateHostUsmVector<int>(queue_, kVecSize);
  queue_.memset(host_vector.get(), 0, kVecSize * sizeof(int)).wait();
  FinalizeCollection();
  EXPECT_EQ(std::size(record_storage_.memfill_records), 1);
  const auto memtype = record_storage_.memfill_records[0]->_mem_type;
  EXPECT_EQ(memtype, PTI_VIEW_MEMORY_TYPE_HOST);
  EXPECT_THAT(ptiViewMemoryTypeToString(memtype),
              ::testing::AllOf(::testing::HasSubstr(kSubstringMemoryTypeHost),
                               ::testing::HasSubstr(kSubstringMemoryTypeDriverAllocated)));
}

TEST_F(MemoryOperationTest, SuccessfulMemCopyUserAllocated) {
  InitCollection();
  EnableViews(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  constexpr std::size_t kVecSize = 64;

  // Force M2D with use_host_ptr and doing a memcpy
  auto host_vector = std::vector<int>(kVecSize, 1);
  {
    sycl::buffer<int, 1> buf(host_vector.data(), sycl::range<1>(kVecSize),
                             {sycl::property::buffer::use_host_ptr()});

    queue_
        .submit([&](sycl::handler& cgh) {
          auto acc = buf.get_access<sycl::access::mode::read_write>(cgh);
          cgh.parallel_for(sycl::range<1>(kVecSize),
                           [=](sycl::id<1> idx) { acc[idx] = acc[idx] + 1; });
        })
        .wait();

    buf.get_host_access();

    EXPECT_TRUE(
        std::all_of(host_vector.begin(), host_vector.end(), [](int idx) { return idx == 2; }));
  }
  FinalizeCollection();
  std::sort(record_storage_.memcpy_records.begin(), record_storage_.memcpy_records.end(),
            [](const auto* first, const auto* second) {
              return first->_start_timestamp < second->_start_timestamp;
            });
  EXPECT_EQ(std::size(record_storage_.memcpy_records), 2);
  EXPECT_EQ(record_storage_.memcpy_records[0]->_mem_dst, PTI_VIEW_MEMORY_TYPE_DEVICE);
  EXPECT_EQ(record_storage_.memcpy_records[0]->_memcpy_type,
            pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2D);
  const auto memtype = record_storage_.memcpy_records[0]->_mem_src;
  EXPECT_EQ(memtype, PTI_VIEW_MEMORY_TYPE_MEMORY);
  EXPECT_THAT(ptiViewMemoryTypeToString(memtype),
              ::testing::AllOf(::testing::HasSubstr(kSubstringMemoryTypeHost),
                               ::testing::HasSubstr(kSubstringMemoryTypeUserAllocated)));
}

TEST(MemoryTypeStringificationTest, ToStringReturnsCorrectStringsForAllMemoryTypes) {
  EXPECT_THAT(ptiViewMemoryTypeToString(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED),
              ::testing::HasSubstr(kSubstringMemoryTypeShared));
  EXPECT_THAT(ptiViewMemoryTypeToString(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST),
              ::testing::AllOf(::testing::HasSubstr(kSubstringMemoryTypeHost),
                               ::testing::HasSubstr(kSubstringMemoryTypeDriverAllocated)));
  EXPECT_THAT(ptiViewMemoryTypeToString(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY),
              ::testing::AllOf(::testing::HasSubstr(kSubstringMemoryTypeHost),
                               ::testing::HasSubstr(kSubstringMemoryTypeUserAllocated)));
  EXPECT_THAT(ptiViewMemoryTypeToString(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE),
              ::testing::HasSubstr(kSubstringMemoryTypeDevice));
}

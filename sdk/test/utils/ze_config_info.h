#ifndef TEST_UTILS_ZE_CONFIG_INFO_H_
#define TEST_UTILS_ZE_CONFIG_INFO_H_

#include <level_zero/ze_api.h>

#include <stdexcept>
#include <string>

namespace pti::test::utils::level_zero {
[[nodiscard]] inline bool CheckIntegratedGraphics(ze_device_handle_t device) {
  ze_device_properties_t device_props{};
  auto return_cd = zeDeviceGetProperties(device, &device_props);
  if (return_cd != ZE_RESULT_SUCCESS) {
    std::string err_msg =
        "Failed to get device properties, device returned: " + std::to_string(return_cd);
    throw std::runtime_error(err_msg);
  }
  if (device_props.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED) {
    return true;
  }
  return false;
}

[[nodiscard]] inline int GetGroupOrdinals(ze_device_handle_t device, uint32_t& computeOrdinal,
                                          uint32_t& copyOrdinal) {
  // Discover all command queue groups
  uint32_t cmdqueue_group_count = 0;
  ze_result_t status =
      zeDeviceGetCommandQueueGroupProperties(device, &cmdqueue_group_count, nullptr);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  std::vector<ze_command_queue_group_properties_t> cmdqueue_group_props(cmdqueue_group_count);
  for (auto& prop : cmdqueue_group_props) {
    prop.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_GROUP_PROPERTIES;
    prop.pNext = nullptr;
  }
  status = zeDeviceGetCommandQueueGroupProperties(device, &cmdqueue_group_count,
                                                  cmdqueue_group_props.data());
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  // Find command queues that support compute and copy
  computeOrdinal = cmdqueue_group_count;
  copyOrdinal = cmdqueue_group_count;
  for (uint32_t i = 0; i < cmdqueue_group_count; ++i) {
    if (cmdqueue_group_props[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
      computeOrdinal = i;
    }
    if (cmdqueue_group_props[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) {
      copyOrdinal = i;
    }
  }
  if (computeOrdinal == cmdqueue_group_count || copyOrdinal == cmdqueue_group_count) {
    std::cout << "No compute or copy command queue group found" << std::endl;
    return 1;
  }
  return 0;
}

}  // namespace pti::test::utils::level_zero

#endif  // TEST_UTILS_ZE_CONFIG_INFO_H_

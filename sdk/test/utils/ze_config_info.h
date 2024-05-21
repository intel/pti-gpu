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
}  // namespace pti::test::utils::level_zero

#endif  // TEST_UTILS_ZE_CONFIG_INFO_H_

//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
#include <string>

#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <drm/i915_drm.h>
#include <xf86drm.h>

#include "pti_assert.h"

#define MAX_STR_LEN 128
#define PERF_GUID_LENGTH 37
#define PERF_REG_OFFSET 0xE458

int OpenDrm() {
  int fd = drmOpenWithType("i915", NULL, DRM_NODE_RENDER);
  if (fd < 0) {
    fd = drmOpenWithType("i915", NULL, DRM_NODE_PRIMARY);
  }
  return fd;
}

void CloseDrm(int fd) {
  drmClose(fd);
}

int SendIoctl(int fd, unsigned request, void* arg) {
  return drmIoctl(fd, request, arg);
}

std::string GetGuid(const std::vector<unsigned>& regs) {
  std::string regs_string;
  for (size_t i = 0; i < regs.size(); ++i) {
    regs_string += regs[i];
  }

  char guid[PERF_GUID_LENGTH] = { 0 };
  std::hash<std::string> hash;
  snprintf(guid, sizeof(guid), "%08x-%04x-%04x-%04x-%012x", 0, 0, 0, 0,
    static_cast<uint32_t>(hash(regs_string)));
  return guid;
}

std::string ConfigureRegisters(int fd, unsigned reg_value, int& config_id) {
  std::vector<unsigned> flex_regs(2);
  flex_regs[0] = PERF_REG_OFFSET;
  flex_regs[1] = reg_value;

  std::string guid = GetGuid(flex_regs);

  struct drm_i915_perf_oa_config param = {0,};
  PTI_ASSERT(sizeof(param.uuid) == PERF_GUID_LENGTH - 1);
  PTI_ASSERT(flex_regs.size() % 2 == 0);
  
  memcpy(param.uuid, guid.c_str(), sizeof(param.uuid));
  
  param.boolean_regs_ptr = 0;
  param.mux_regs_ptr = 0;
  param.flex_regs_ptr = reinterpret_cast<uint64_t>(flex_regs.data());
  
  param.n_boolean_regs = 0;
  param.n_mux_regs = 0;
  PTI_ASSERT(flex_regs.size() < (std::numeric_limits<uint32_t>::max)());
  param.n_flex_regs = static_cast<uint32_t>(flex_regs.size() / 2);

  config_id = SendIoctl(fd, DRM_IOCTL_I915_PERF_ADD_CONFIG, &param);
  return guid;
}

int OpenPerfStream(int fd, int config_id) {
  PTI_ASSERT(config_id >= 0);
  uint64_t properties[] = {
    DRM_I915_PERF_PROP_SAMPLE_OA, 1,
    DRM_I915_PERF_PROP_OA_METRICS_SET, static_cast<uint64_t>(config_id),
    DRM_I915_PERF_PROP_OA_FORMAT, I915_OA_FORMAT_A32u40_A4u32_B8_C8 };
  
  struct drm_i915_perf_open_param param = { 0, };
  param.flags = 0;
  param.flags |= I915_PERF_FLAG_FD_CLOEXEC;
  param.flags |= I915_PERF_FLAG_FD_NONBLOCK;
  param.properties_ptr = reinterpret_cast<uint64_t>(properties);
  param.num_properties = sizeof(properties) / (2 * sizeof(uint64_t));

  return SendIoctl(fd, DRM_IOCTL_I915_PERF_OPEN, &param);
}

void ClosePerfStream(int fd) {
  int status = close(fd);
  PTI_ASSERT(status == 0);
}

int GetDrmCardNumber(int fd) {
  struct stat file_info;
  if (fstat(fd, &file_info)) {
    return -1;
  }

  int major_number = major(file_info.st_rdev);
  int minor_number = minor(file_info.st_rdev);

  char drm_path[MAX_STR_LEN] = { 0 };
  snprintf(drm_path, sizeof(drm_path), "/sys/dev/char/%d:%d/device/drm",
    major_number, minor_number);

  DIR* drm_dir = opendir(drm_path);
  PTI_ASSERT(drm_dir != nullptr);

  dirent* entry = nullptr;
  int card = -1;
  while ((entry = readdir(drm_dir)) != nullptr) {
    if (entry->d_type == DT_DIR && strncmp(entry->d_name, "card", 4) == 0) {
      card = strtol(entry->d_name + 4, nullptr, 10);
      break;
    }
  }
  
  closedir(drm_dir);
  return card;
}

int GetPerfConfigId(int fd, int card, const char* guid) {
  char file_path[MAX_STR_LEN] = { 0 };
  snprintf(file_path, sizeof(file_path),
           "/sys/class/drm/card%d/metrics/%s/id", card, guid);

  std::ifstream file {file_path};
  if (!file) {
    return -1;
  }

  std::string buffer;
  std::getline(file, buffer);
  PTI_ASSERT(buffer.size() > 0);

  return strtol(buffer.c_str(), 0, 0);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Specify perfmon register value" << std::endl;
    return 0;
  }

  unsigned value = std::stoul(argv[1]);

  bool test_mode = false;
  if (argc > 2) {
    if (strcmp(argv[2], "-t") == 0) {
      test_mode = true;
    }
  }

  int fd = OpenDrm();
  if (fd < 0) {
    std::cout << "Can't open DRM for i915 driver (status: " <<
      -fd << ")" << std::endl;
    return 0;
  }

  int config_id = -1;
  std::string guid = ConfigureRegisters(fd, value, config_id);
  if (config_id == -1) {
    if (errno == EADDRINUSE) {
      std::cout << "Configuration with the given GUID is already added" <<
        std::endl;
      
      int card = GetDrmCardNumber(fd);
      PTI_ASSERT(card >= 0);
      config_id = GetPerfConfigId(fd, card, guid.c_str());
      PTI_ASSERT(config_id >= 0);
    } else {
      std::cout << "Adding i915 perf configuration is failed (" <<
        errno << ": " << strerror(errno) << ")" << std::endl;
      CloseDrm(fd);
      return 0;
    }
  }

  int stream_fd = OpenPerfStream(fd, config_id);
  if (stream_fd == -1) {
    std::cout << "Opening i915 perf stream is failed (" <<
      errno << ": " << strerror(errno) << ")" << std::endl;
    CloseDrm(fd);
    return 0;
  }

  std::cout << "GPU PefMon configuration is completed" << std::endl;
  std::cout << "Press ENTER to deconfigure the driver..." << std::endl;
  if (!test_mode) {
    std::cin.get();
  }

  ClosePerfStream(stream_fd);
  CloseDrm(fd);
  return 0;
}

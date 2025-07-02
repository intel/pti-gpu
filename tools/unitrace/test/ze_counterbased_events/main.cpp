//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================


#include <stdio.h>
#include <string.h>
#include <string>
#include "ze_utils.h"

static void *allocSharedMem(ze_context_handle_t hCtx, ze_device_handle_t hDev, size_t size)
{
    ze_host_mem_alloc_desc_t h_desc = {
        .stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC,
        .pNext = nullptr,
        .flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED
    };
    ze_device_mem_alloc_desc_t d_desc = {
        .stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
        .pNext = nullptr,
        .flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED,
        .ordinal = 0
    };

    void *buf = nullptr;

    ze_result_t rc = zeMemAllocShared(hCtx, &d_desc, &h_desc, size, 4096, hDev, &buf);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeMemAllocDevice rc=%d\n", rc);
        return nullptr;
    }

    return buf;
}

int main(int argc, char* argv[])
{
  uint32_t buf_size = 1024 * 1024;
  uint32_t niters = 4;

  for (int i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-size") && i+1 < argc) {
      buf_size = std::stoi(argv[++i]);
    }
  }

  ze_result_t rc = ZE_RESULT_SUCCESS;
  rc = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  if (rc != ZE_RESULT_SUCCESS) {
      printf("[ERROR] Failed to init GPU device: 0x%x\n", rc);
      return -1;
  }

  ze_device_handle_t device = utils::ze::GetGpuDevice();
  ze_driver_handle_t driver = utils::ze::GetGpuDriver();
  if (device == nullptr || driver == nullptr) {
      printf("[ERROR] Unable to find GPU device\n");
      return -1;
  }

  ze_context_handle_t context = utils::ze::GetContext(driver);

  float *in_buf = (float *)allocSharedMem(context, device, buf_size * sizeof(float));
  float *out_buf = (float *)allocSharedMem(context, device, buf_size * sizeof(float));

  for (uint32_t i = 0; i < buf_size; i++) {
      in_buf[i] = (float)(i & 100);
      out_buf[i] = 255.0f;
  }

  _ze_event_pool_counter_based_exp_desc_t cbased = {
    .stype = ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC,
	.pNext = nullptr,
	.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE
  };

  ze_event_pool_desc_t ctr_pool_desc = {
      .stype = ZE_STRUCTURE_TYPE_EVENT_POOL_DESC,
      .pNext = &cbased,
      .flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
      .count = niters
  };

  ze_event_pool_handle_t ctr_evPool;
  rc = zeEventPoolCreate(context, &ctr_pool_desc, 1, &device, &ctr_evPool);
  if (rc != ZE_RESULT_SUCCESS) {
      printf("zeEventPoolCreate rc=%d\n", rc);
      return -1;
  }

  ze_command_queue_desc_t q_desc = {
      .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
      .pNext = nullptr,
      .ordinal = 0,
      .index = 0,
      .flags = ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
      .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL
  };

  ze_command_list_handle_t cl;

  rc = zeCommandListCreateImmediate(context, device, &q_desc, &cl);
  if (rc != ZE_RESULT_SUCCESS) {
      printf("zeCommandListCreateImmediate rc=%d\n", rc);
      return -1;
  }

  std::vector<ze_event_handle_t> ev(niters);
  for (unsigned int i = 0; i < niters; i++) {
      ze_event_desc_t ev_desc = {
          .stype = ZE_STRUCTURE_TYPE_EVENT_DESC,
          .pNext = nullptr,
          .index = i,
          .signal = ZE_EVENT_SCOPE_FLAG_DEVICE,
          .wait = ZE_EVENT_SCOPE_FLAG_HOST,
      };
      rc = zeEventCreate(ctr_evPool, &ev_desc, &ev[i]);
      if (rc != ZE_RESULT_SUCCESS) {
          printf("zeEventCreate rc=%d\n", rc);
          return -1;
      }
  }

  uint32_t chunkSize = buf_size / niters;
  uint32_t start = 0;
  for (unsigned int i = 0; i < niters; i++) {
    uint32_t size = start + chunkSize > buf_size ? buf_size - start : chunkSize;

    rc = zeCommandListAppendMemoryCopy(cl,
                                      &out_buf[start], &in_buf[start],
                                       size * sizeof(float),
                                       ev[0], 0, nullptr);
    if (rc != ZE_RESULT_SUCCESS) {
        printf("zeEventCreate rc=%d\n", rc);
        return -1;
    }

    start += chunkSize;
  }

  zeEventHostSynchronize(ev[0], UINT64_MAX);

  for (uint32_t i = 1; i < 128 && i <= buf_size; i++) {
    if (out_buf[buf_size - i] != in_buf[buf_size - i]) {
      printf("[ERROR] Mismatch at index %u %f expected: %f\n", buf_size - i, out_buf[buf_size - i], in_buf[buf_size - i]);
    }
  }

  zeCommandListDestroy(cl);
  for (unsigned int i = 0; i < niters; i++)
    zeEventDestroy(ev[i]);
  zeEventPoolDestroy(ctr_evPool);
  zeContextDestroy(context);

  return 0;
}

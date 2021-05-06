//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_CALLBACKS_H_
#define PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_CALLBACKS_H_

#include "pti_assert.h"

static void zeInitOnEnter(
    ze_init_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeInitOnExit(
    ze_init_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeInit", time);
}

static void zeDriverGetOnEnter(
    ze_driver_get_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDriverGetOnExit(
    ze_driver_get_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDriverGet", time);
}

static void zeDriverGetApiVersionOnEnter(
    ze_driver_get_api_version_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDriverGetApiVersionOnExit(
    ze_driver_get_api_version_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDriverGetApiVersion", time);
}

static void zeDriverGetPropertiesOnEnter(
    ze_driver_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDriverGetPropertiesOnExit(
    ze_driver_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDriverGetProperties", time);
}

static void zeDriverGetIpcPropertiesOnEnter(
    ze_driver_get_ipc_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDriverGetIpcPropertiesOnExit(
    ze_driver_get_ipc_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDriverGetIpcProperties", time);
}

static void zeDriverGetExtensionPropertiesOnEnter(
    ze_driver_get_extension_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDriverGetExtensionPropertiesOnExit(
    ze_driver_get_extension_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDriverGetExtensionProperties", time);
}

static void zeDeviceGetOnEnter(
    ze_device_get_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetOnExit(
    ze_device_get_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGet", time);
}

static void zeDeviceGetSubDevicesOnEnter(
    ze_device_get_sub_devices_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetSubDevicesOnExit(
    ze_device_get_sub_devices_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetSubDevices", time);
}

static void zeDeviceGetPropertiesOnEnter(
    ze_device_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetPropertiesOnExit(
    ze_device_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetProperties", time);
}

static void zeDeviceGetComputePropertiesOnEnter(
    ze_device_get_compute_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetComputePropertiesOnExit(
    ze_device_get_compute_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetComputeProperties", time);
}

static void zeDeviceGetModulePropertiesOnEnter(
    ze_device_get_module_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetModulePropertiesOnExit(
    ze_device_get_module_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetModuleProperties", time);
}

static void zeDeviceGetCommandQueueGroupPropertiesOnEnter(
    ze_device_get_command_queue_group_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetCommandQueueGroupPropertiesOnExit(
    ze_device_get_command_queue_group_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetCommandQueueGroupProperties", time);
}

static void zeDeviceGetMemoryPropertiesOnEnter(
    ze_device_get_memory_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetMemoryPropertiesOnExit(
    ze_device_get_memory_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetMemoryProperties", time);
}

static void zeDeviceGetMemoryAccessPropertiesOnEnter(
    ze_device_get_memory_access_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetMemoryAccessPropertiesOnExit(
    ze_device_get_memory_access_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetMemoryAccessProperties", time);
}

static void zeDeviceGetCachePropertiesOnEnter(
    ze_device_get_cache_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetCachePropertiesOnExit(
    ze_device_get_cache_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetCacheProperties", time);
}

static void zeDeviceGetImagePropertiesOnEnter(
    ze_device_get_image_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetImagePropertiesOnExit(
    ze_device_get_image_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetImageProperties", time);
}

static void zeDeviceGetExternalMemoryPropertiesOnEnter(
    ze_device_get_external_memory_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetExternalMemoryPropertiesOnExit(
    ze_device_get_external_memory_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetExternalMemoryProperties", time);
}

static void zeDeviceGetP2PPropertiesOnEnter(
    ze_device_get_p2_p_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetP2PPropertiesOnExit(
    ze_device_get_p2_p_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetP2PProperties", time);
}

static void zeDeviceCanAccessPeerOnEnter(
    ze_device_can_access_peer_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceCanAccessPeerOnExit(
    ze_device_can_access_peer_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceCanAccessPeer", time);
}

static void zeDeviceGetStatusOnEnter(
    ze_device_get_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeDeviceGetStatusOnExit(
    ze_device_get_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeDeviceGetStatus", time);
}

static void zeContextCreateOnEnter(
    ze_context_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextCreateOnExit(
    ze_context_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextCreate", time);
}

static void zeContextDestroyOnEnter(
    ze_context_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextDestroyOnExit(
    ze_context_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextDestroy", time);
}

static void zeContextGetStatusOnEnter(
    ze_context_get_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextGetStatusOnExit(
    ze_context_get_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextGetStatus", time);
}

static void zeContextSystemBarrierOnEnter(
    ze_context_system_barrier_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextSystemBarrierOnExit(
    ze_context_system_barrier_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextSystemBarrier", time);
}

static void zeContextMakeMemoryResidentOnEnter(
    ze_context_make_memory_resident_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextMakeMemoryResidentOnExit(
    ze_context_make_memory_resident_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextMakeMemoryResident", time);
}

static void zeContextEvictMemoryOnEnter(
    ze_context_evict_memory_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextEvictMemoryOnExit(
    ze_context_evict_memory_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextEvictMemory", time);
}

static void zeContextMakeImageResidentOnEnter(
    ze_context_make_image_resident_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextMakeImageResidentOnExit(
    ze_context_make_image_resident_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextMakeImageResident", time);
}

static void zeContextEvictImageOnEnter(
    ze_context_evict_image_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeContextEvictImageOnExit(
    ze_context_evict_image_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeContextEvictImage", time);
}

static void zeCommandQueueCreateOnEnter(
    ze_command_queue_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandQueueCreateOnExit(
    ze_command_queue_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandQueueCreate", time);
}

static void zeCommandQueueDestroyOnEnter(
    ze_command_queue_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandQueueDestroyOnExit(
    ze_command_queue_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandQueueDestroy", time);
}

static void zeCommandQueueExecuteCommandListsOnEnter(
    ze_command_queue_execute_command_lists_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandQueueExecuteCommandListsOnExit(
    ze_command_queue_execute_command_lists_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandQueueExecuteCommandLists", time);
}

static void zeCommandQueueSynchronizeOnEnter(
    ze_command_queue_synchronize_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandQueueSynchronizeOnExit(
    ze_command_queue_synchronize_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandQueueSynchronize", time);
}

static void zeCommandListCreateOnEnter(
    ze_command_list_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListCreateOnExit(
    ze_command_list_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListCreate", time);
}

static void zeCommandListCreateImmediateOnEnter(
    ze_command_list_create_immediate_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListCreateImmediateOnExit(
    ze_command_list_create_immediate_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListCreateImmediate", time);
}

static void zeCommandListDestroyOnEnter(
    ze_command_list_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListDestroyOnExit(
    ze_command_list_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListDestroy", time);
}

static void zeCommandListCloseOnEnter(
    ze_command_list_close_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListCloseOnExit(
    ze_command_list_close_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListClose", time);
}

static void zeCommandListResetOnEnter(
    ze_command_list_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListResetOnExit(
    ze_command_list_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListReset", time);
}

static void zeCommandListAppendWriteGlobalTimestampOnEnter(
    ze_command_list_append_write_global_timestamp_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendWriteGlobalTimestampOnExit(
    ze_command_list_append_write_global_timestamp_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendWriteGlobalTimestamp", time);
}

static void zeCommandListAppendBarrierOnEnter(
    ze_command_list_append_barrier_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendBarrierOnExit(
    ze_command_list_append_barrier_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendBarrier", time);
}

static void zeCommandListAppendMemoryRangesBarrierOnEnter(
    ze_command_list_append_memory_ranges_barrier_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemoryRangesBarrierOnExit(
    ze_command_list_append_memory_ranges_barrier_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemoryRangesBarrier", time);
}

static void zeCommandListAppendMemoryCopyOnEnter(
    ze_command_list_append_memory_copy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemoryCopyOnExit(
    ze_command_list_append_memory_copy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemoryCopy", time);
}

static void zeCommandListAppendMemoryFillOnEnter(
    ze_command_list_append_memory_fill_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemoryFillOnExit(
    ze_command_list_append_memory_fill_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemoryFill", time);
}

static void zeCommandListAppendMemoryCopyRegionOnEnter(
    ze_command_list_append_memory_copy_region_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemoryCopyRegionOnExit(
    ze_command_list_append_memory_copy_region_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemoryCopyRegion", time);
}

static void zeCommandListAppendMemoryCopyFromContextOnEnter(
    ze_command_list_append_memory_copy_from_context_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemoryCopyFromContextOnExit(
    ze_command_list_append_memory_copy_from_context_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemoryCopyFromContext", time);
}

static void zeCommandListAppendImageCopyOnEnter(
    ze_command_list_append_image_copy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendImageCopyOnExit(
    ze_command_list_append_image_copy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendImageCopy", time);
}

static void zeCommandListAppendImageCopyRegionOnEnter(
    ze_command_list_append_image_copy_region_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendImageCopyRegionOnExit(
    ze_command_list_append_image_copy_region_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendImageCopyRegion", time);
}

static void zeCommandListAppendImageCopyToMemoryOnEnter(
    ze_command_list_append_image_copy_to_memory_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendImageCopyToMemoryOnExit(
    ze_command_list_append_image_copy_to_memory_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendImageCopyToMemory", time);
}

static void zeCommandListAppendImageCopyFromMemoryOnEnter(
    ze_command_list_append_image_copy_from_memory_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendImageCopyFromMemoryOnExit(
    ze_command_list_append_image_copy_from_memory_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendImageCopyFromMemory", time);
}

static void zeCommandListAppendMemoryPrefetchOnEnter(
    ze_command_list_append_memory_prefetch_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemoryPrefetchOnExit(
    ze_command_list_append_memory_prefetch_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemoryPrefetch", time);
}

static void zeCommandListAppendMemAdviseOnEnter(
    ze_command_list_append_mem_advise_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendMemAdviseOnExit(
    ze_command_list_append_mem_advise_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendMemAdvise", time);
}

static void zeCommandListAppendSignalEventOnEnter(
    ze_command_list_append_signal_event_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendSignalEventOnExit(
    ze_command_list_append_signal_event_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendSignalEvent", time);
}

static void zeCommandListAppendWaitOnEventsOnEnter(
    ze_command_list_append_wait_on_events_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendWaitOnEventsOnExit(
    ze_command_list_append_wait_on_events_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendWaitOnEvents", time);
}

static void zeCommandListAppendEventResetOnEnter(
    ze_command_list_append_event_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendEventResetOnExit(
    ze_command_list_append_event_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendEventReset", time);
}

static void zeCommandListAppendQueryKernelTimestampsOnEnter(
    ze_command_list_append_query_kernel_timestamps_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendQueryKernelTimestampsOnExit(
    ze_command_list_append_query_kernel_timestamps_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendQueryKernelTimestamps", time);
}

static void zeCommandListAppendLaunchKernelOnEnter(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendLaunchKernelOnExit(
    ze_command_list_append_launch_kernel_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendLaunchKernel", time);
}

static void zeCommandListAppendLaunchCooperativeKernelOnEnter(
    ze_command_list_append_launch_cooperative_kernel_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendLaunchCooperativeKernelOnExit(
    ze_command_list_append_launch_cooperative_kernel_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendLaunchCooperativeKernel", time);
}

static void zeCommandListAppendLaunchKernelIndirectOnEnter(
    ze_command_list_append_launch_kernel_indirect_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendLaunchKernelIndirectOnExit(
    ze_command_list_append_launch_kernel_indirect_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendLaunchKernelIndirect", time);
}

static void zeCommandListAppendLaunchMultipleKernelsIndirectOnEnter(
    ze_command_list_append_launch_multiple_kernels_indirect_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeCommandListAppendLaunchMultipleKernelsIndirectOnExit(
    ze_command_list_append_launch_multiple_kernels_indirect_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeCommandListAppendLaunchMultipleKernelsIndirect", time);
}

static void zeFenceCreateOnEnter(
    ze_fence_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeFenceCreateOnExit(
    ze_fence_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeFenceCreate", time);
}

static void zeFenceDestroyOnEnter(
    ze_fence_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeFenceDestroyOnExit(
    ze_fence_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeFenceDestroy", time);
}

static void zeFenceHostSynchronizeOnEnter(
    ze_fence_host_synchronize_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeFenceHostSynchronizeOnExit(
    ze_fence_host_synchronize_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeFenceHostSynchronize", time);
}

static void zeFenceQueryStatusOnEnter(
    ze_fence_query_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeFenceQueryStatusOnExit(
    ze_fence_query_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeFenceQueryStatus", time);
}

static void zeFenceResetOnEnter(
    ze_fence_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeFenceResetOnExit(
    ze_fence_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeFenceReset", time);
}

static void zeEventPoolCreateOnEnter(
    ze_event_pool_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventPoolCreateOnExit(
    ze_event_pool_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventPoolCreate", time);
}

static void zeEventPoolDestroyOnEnter(
    ze_event_pool_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventPoolDestroyOnExit(
    ze_event_pool_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventPoolDestroy", time);
}

static void zeEventPoolGetIpcHandleOnEnter(
    ze_event_pool_get_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventPoolGetIpcHandleOnExit(
    ze_event_pool_get_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventPoolGetIpcHandle", time);
}

static void zeEventPoolOpenIpcHandleOnEnter(
    ze_event_pool_open_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventPoolOpenIpcHandleOnExit(
    ze_event_pool_open_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventPoolOpenIpcHandle", time);
}

static void zeEventPoolCloseIpcHandleOnEnter(
    ze_event_pool_close_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventPoolCloseIpcHandleOnExit(
    ze_event_pool_close_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventPoolCloseIpcHandle", time);
}

static void zeEventCreateOnEnter(
    ze_event_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventCreateOnExit(
    ze_event_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventCreate", time);
}

static void zeEventDestroyOnEnter(
    ze_event_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventDestroyOnExit(
    ze_event_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventDestroy", time);
}

static void zeEventHostSignalOnEnter(
    ze_event_host_signal_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventHostSignalOnExit(
    ze_event_host_signal_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventHostSignal", time);
}

static void zeEventHostSynchronizeOnEnter(
    ze_event_host_synchronize_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventHostSynchronizeOnExit(
    ze_event_host_synchronize_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventHostSynchronize", time);
}

static void zeEventQueryStatusOnEnter(
    ze_event_query_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventQueryStatusOnExit(
    ze_event_query_status_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventQueryStatus", time);
}

static void zeEventHostResetOnEnter(
    ze_event_host_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventHostResetOnExit(
    ze_event_host_reset_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventHostReset", time);
}

static void zeEventQueryKernelTimestampOnEnter(
    ze_event_query_kernel_timestamp_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeEventQueryKernelTimestampOnExit(
    ze_event_query_kernel_timestamp_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeEventQueryKernelTimestamp", time);
}

static void zeImageGetPropertiesOnEnter(
    ze_image_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeImageGetPropertiesOnExit(
    ze_image_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeImageGetProperties", time);
}

static void zeImageCreateOnEnter(
    ze_image_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeImageCreateOnExit(
    ze_image_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeImageCreate", time);
}

static void zeImageDestroyOnEnter(
    ze_image_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeImageDestroyOnExit(
    ze_image_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeImageDestroy", time);
}

static void zeModuleCreateOnEnter(
    ze_module_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleCreateOnExit(
    ze_module_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleCreate", time);
}

static void zeModuleDestroyOnEnter(
    ze_module_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleDestroyOnExit(
    ze_module_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleDestroy", time);
}

static void zeModuleDynamicLinkOnEnter(
    ze_module_dynamic_link_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleDynamicLinkOnExit(
    ze_module_dynamic_link_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleDynamicLink", time);
}

static void zeModuleGetNativeBinaryOnEnter(
    ze_module_get_native_binary_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleGetNativeBinaryOnExit(
    ze_module_get_native_binary_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleGetNativeBinary", time);
}

static void zeModuleGetGlobalPointerOnEnter(
    ze_module_get_global_pointer_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleGetGlobalPointerOnExit(
    ze_module_get_global_pointer_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleGetGlobalPointer", time);
}

static void zeModuleGetKernelNamesOnEnter(
    ze_module_get_kernel_names_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleGetKernelNamesOnExit(
    ze_module_get_kernel_names_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleGetKernelNames", time);
}

static void zeModuleGetPropertiesOnEnter(
    ze_module_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleGetPropertiesOnExit(
    ze_module_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleGetProperties", time);
}

static void zeModuleGetFunctionPointerOnEnter(
    ze_module_get_function_pointer_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleGetFunctionPointerOnExit(
    ze_module_get_function_pointer_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleGetFunctionPointer", time);
}

static void zeModuleBuildLogDestroyOnEnter(
    ze_module_build_log_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleBuildLogDestroyOnExit(
    ze_module_build_log_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleBuildLogDestroy", time);
}

static void zeModuleBuildLogGetStringOnEnter(
    ze_module_build_log_get_string_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeModuleBuildLogGetStringOnExit(
    ze_module_build_log_get_string_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeModuleBuildLogGetString", time);
}

static void zeKernelCreateOnEnter(
    ze_kernel_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelCreateOnExit(
    ze_kernel_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelCreate", time);
}

static void zeKernelDestroyOnEnter(
    ze_kernel_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelDestroyOnExit(
    ze_kernel_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelDestroy", time);
}

static void zeKernelSetCacheConfigOnEnter(
    ze_kernel_set_cache_config_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelSetCacheConfigOnExit(
    ze_kernel_set_cache_config_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelSetCacheConfig", time);
}

static void zeKernelSetGroupSizeOnEnter(
    ze_kernel_set_group_size_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelSetGroupSizeOnExit(
    ze_kernel_set_group_size_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelSetGroupSize", time);
}

static void zeKernelSuggestGroupSizeOnEnter(
    ze_kernel_suggest_group_size_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelSuggestGroupSizeOnExit(
    ze_kernel_suggest_group_size_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelSuggestGroupSize", time);
}

static void zeKernelSuggestMaxCooperativeGroupCountOnEnter(
    ze_kernel_suggest_max_cooperative_group_count_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelSuggestMaxCooperativeGroupCountOnExit(
    ze_kernel_suggest_max_cooperative_group_count_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelSuggestMaxCooperativeGroupCount", time);
}

static void zeKernelSetArgumentValueOnEnter(
    ze_kernel_set_argument_value_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelSetArgumentValueOnExit(
    ze_kernel_set_argument_value_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelSetArgumentValue", time);
}

static void zeKernelSetIndirectAccessOnEnter(
    ze_kernel_set_indirect_access_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelSetIndirectAccessOnExit(
    ze_kernel_set_indirect_access_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelSetIndirectAccess", time);
}

static void zeKernelGetIndirectAccessOnEnter(
    ze_kernel_get_indirect_access_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelGetIndirectAccessOnExit(
    ze_kernel_get_indirect_access_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelGetIndirectAccess", time);
}

static void zeKernelGetSourceAttributesOnEnter(
    ze_kernel_get_source_attributes_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelGetSourceAttributesOnExit(
    ze_kernel_get_source_attributes_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelGetSourceAttributes", time);
}

static void zeKernelGetPropertiesOnEnter(
    ze_kernel_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelGetPropertiesOnExit(
    ze_kernel_get_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelGetProperties", time);
}

static void zeKernelGetNameOnEnter(
    ze_kernel_get_name_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeKernelGetNameOnExit(
    ze_kernel_get_name_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeKernelGetName", time);
}

static void zeSamplerCreateOnEnter(
    ze_sampler_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeSamplerCreateOnExit(
    ze_sampler_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeSamplerCreate", time);
}

static void zeSamplerDestroyOnEnter(
    ze_sampler_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeSamplerDestroyOnExit(
    ze_sampler_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeSamplerDestroy", time);
}

static void zePhysicalMemCreateOnEnter(
    ze_physical_mem_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zePhysicalMemCreateOnExit(
    ze_physical_mem_create_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zePhysicalMemCreate", time);
}

static void zePhysicalMemDestroyOnEnter(
    ze_physical_mem_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zePhysicalMemDestroyOnExit(
    ze_physical_mem_destroy_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zePhysicalMemDestroy", time);
}

static void zeMemAllocSharedOnEnter(
    ze_mem_alloc_shared_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemAllocSharedOnExit(
    ze_mem_alloc_shared_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemAllocShared", time);
}

static void zeMemAllocDeviceOnEnter(
    ze_mem_alloc_device_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemAllocDeviceOnExit(
    ze_mem_alloc_device_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemAllocDevice", time);
}

static void zeMemAllocHostOnEnter(
    ze_mem_alloc_host_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemAllocHostOnExit(
    ze_mem_alloc_host_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemAllocHost", time);
}

static void zeMemFreeOnEnter(
    ze_mem_free_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemFreeOnExit(
    ze_mem_free_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemFree", time);
}

static void zeMemGetAllocPropertiesOnEnter(
    ze_mem_get_alloc_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemGetAllocPropertiesOnExit(
    ze_mem_get_alloc_properties_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemGetAllocProperties", time);
}

static void zeMemGetAddressRangeOnEnter(
    ze_mem_get_address_range_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemGetAddressRangeOnExit(
    ze_mem_get_address_range_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemGetAddressRange", time);
}

static void zeMemGetIpcHandleOnEnter(
    ze_mem_get_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemGetIpcHandleOnExit(
    ze_mem_get_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemGetIpcHandle", time);
}

static void zeMemOpenIpcHandleOnEnter(
    ze_mem_open_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemOpenIpcHandleOnExit(
    ze_mem_open_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemOpenIpcHandle", time);
}

static void zeMemCloseIpcHandleOnEnter(
    ze_mem_close_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeMemCloseIpcHandleOnExit(
    ze_mem_close_ipc_handle_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeMemCloseIpcHandle", time);
}

static void zeVirtualMemReserveOnEnter(
    ze_virtual_mem_reserve_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemReserveOnExit(
    ze_virtual_mem_reserve_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemReserve", time);
}

static void zeVirtualMemFreeOnEnter(
    ze_virtual_mem_free_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemFreeOnExit(
    ze_virtual_mem_free_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemFree", time);
}

static void zeVirtualMemQueryPageSizeOnEnter(
    ze_virtual_mem_query_page_size_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemQueryPageSizeOnExit(
    ze_virtual_mem_query_page_size_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemQueryPageSize", time);
}

static void zeVirtualMemMapOnEnter(
    ze_virtual_mem_map_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemMapOnExit(
    ze_virtual_mem_map_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemMap", time);
}

static void zeVirtualMemUnmapOnEnter(
    ze_virtual_mem_unmap_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemUnmapOnExit(
    ze_virtual_mem_unmap_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemUnmap", time);
}

static void zeVirtualMemSetAccessAttributeOnEnter(
    ze_virtual_mem_set_access_attribute_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemSetAccessAttributeOnExit(
    ze_virtual_mem_set_access_attribute_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemSetAccessAttribute", time);
}

static void zeVirtualMemGetAccessAttributeOnEnter(
    ze_virtual_mem_get_access_attribute_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  start_time = collector->GetTimestamp();
}

static void zeVirtualMemGetAccessAttributeOnExit(
    ze_virtual_mem_get_access_attribute_params_t* params,
    ze_result_t result,
    void* global_user_data,
    void** instance_user_data) {
  PTI_ASSERT(global_user_data != nullptr);
  ZeApiCollector* collector =
    reinterpret_cast<ZeApiCollector*>(global_user_data);
  uint64_t end_time = collector->GetTimestamp();

  uint64_t& start_time = *reinterpret_cast<uint64_t*>(instance_user_data);
  PTI_ASSERT(start_time > 0);
  PTI_ASSERT(start_time < end_time);
  uint64_t time = end_time - start_time;
  collector->AddFunctionTime("zeVirtualMemGetAccessAttribute", time);
}

static void SetTracingFunctions(zel_tracer_handle_t tracer) {
  zet_core_callbacks_t prologue = {};
  zet_core_callbacks_t epilogue = {};

  prologue.Global.pfnInitCb = zeInitOnEnter;
  epilogue.Global.pfnInitCb = zeInitOnExit;
  prologue.Driver.pfnGetCb = zeDriverGetOnEnter;
  epilogue.Driver.pfnGetCb = zeDriverGetOnExit;
  prologue.Driver.pfnGetApiVersionCb = zeDriverGetApiVersionOnEnter;
  epilogue.Driver.pfnGetApiVersionCb = zeDriverGetApiVersionOnExit;
  prologue.Driver.pfnGetPropertiesCb = zeDriverGetPropertiesOnEnter;
  epilogue.Driver.pfnGetPropertiesCb = zeDriverGetPropertiesOnExit;
  prologue.Driver.pfnGetIpcPropertiesCb = zeDriverGetIpcPropertiesOnEnter;
  epilogue.Driver.pfnGetIpcPropertiesCb = zeDriverGetIpcPropertiesOnExit;
  prologue.Driver.pfnGetExtensionPropertiesCb = zeDriverGetExtensionPropertiesOnEnter;
  epilogue.Driver.pfnGetExtensionPropertiesCb = zeDriverGetExtensionPropertiesOnExit;
  prologue.Device.pfnGetCb = zeDeviceGetOnEnter;
  epilogue.Device.pfnGetCb = zeDeviceGetOnExit;
  prologue.Device.pfnGetSubDevicesCb = zeDeviceGetSubDevicesOnEnter;
  epilogue.Device.pfnGetSubDevicesCb = zeDeviceGetSubDevicesOnExit;
  prologue.Device.pfnGetPropertiesCb = zeDeviceGetPropertiesOnEnter;
  epilogue.Device.pfnGetPropertiesCb = zeDeviceGetPropertiesOnExit;
  prologue.Device.pfnGetComputePropertiesCb = zeDeviceGetComputePropertiesOnEnter;
  epilogue.Device.pfnGetComputePropertiesCb = zeDeviceGetComputePropertiesOnExit;
  prologue.Device.pfnGetModulePropertiesCb = zeDeviceGetModulePropertiesOnEnter;
  epilogue.Device.pfnGetModulePropertiesCb = zeDeviceGetModulePropertiesOnExit;
  prologue.Device.pfnGetCommandQueueGroupPropertiesCb = zeDeviceGetCommandQueueGroupPropertiesOnEnter;
  epilogue.Device.pfnGetCommandQueueGroupPropertiesCb = zeDeviceGetCommandQueueGroupPropertiesOnExit;
  prologue.Device.pfnGetMemoryPropertiesCb = zeDeviceGetMemoryPropertiesOnEnter;
  epilogue.Device.pfnGetMemoryPropertiesCb = zeDeviceGetMemoryPropertiesOnExit;
  prologue.Device.pfnGetMemoryAccessPropertiesCb = zeDeviceGetMemoryAccessPropertiesOnEnter;
  epilogue.Device.pfnGetMemoryAccessPropertiesCb = zeDeviceGetMemoryAccessPropertiesOnExit;
  prologue.Device.pfnGetCachePropertiesCb = zeDeviceGetCachePropertiesOnEnter;
  epilogue.Device.pfnGetCachePropertiesCb = zeDeviceGetCachePropertiesOnExit;
  prologue.Device.pfnGetImagePropertiesCb = zeDeviceGetImagePropertiesOnEnter;
  epilogue.Device.pfnGetImagePropertiesCb = zeDeviceGetImagePropertiesOnExit;
  prologue.Device.pfnGetExternalMemoryPropertiesCb = zeDeviceGetExternalMemoryPropertiesOnEnter;
  epilogue.Device.pfnGetExternalMemoryPropertiesCb = zeDeviceGetExternalMemoryPropertiesOnExit;
  prologue.Device.pfnGetP2PPropertiesCb = zeDeviceGetP2PPropertiesOnEnter;
  epilogue.Device.pfnGetP2PPropertiesCb = zeDeviceGetP2PPropertiesOnExit;
  prologue.Device.pfnCanAccessPeerCb = zeDeviceCanAccessPeerOnEnter;
  epilogue.Device.pfnCanAccessPeerCb = zeDeviceCanAccessPeerOnExit;
  prologue.Device.pfnGetStatusCb = zeDeviceGetStatusOnEnter;
  epilogue.Device.pfnGetStatusCb = zeDeviceGetStatusOnExit;
  prologue.Context.pfnCreateCb = zeContextCreateOnEnter;
  epilogue.Context.pfnCreateCb = zeContextCreateOnExit;
  prologue.Context.pfnDestroyCb = zeContextDestroyOnEnter;
  epilogue.Context.pfnDestroyCb = zeContextDestroyOnExit;
  prologue.Context.pfnGetStatusCb = zeContextGetStatusOnEnter;
  epilogue.Context.pfnGetStatusCb = zeContextGetStatusOnExit;
  prologue.Context.pfnSystemBarrierCb = zeContextSystemBarrierOnEnter;
  epilogue.Context.pfnSystemBarrierCb = zeContextSystemBarrierOnExit;
  prologue.Context.pfnMakeMemoryResidentCb = zeContextMakeMemoryResidentOnEnter;
  epilogue.Context.pfnMakeMemoryResidentCb = zeContextMakeMemoryResidentOnExit;
  prologue.Context.pfnEvictMemoryCb = zeContextEvictMemoryOnEnter;
  epilogue.Context.pfnEvictMemoryCb = zeContextEvictMemoryOnExit;
  prologue.Context.pfnMakeImageResidentCb = zeContextMakeImageResidentOnEnter;
  epilogue.Context.pfnMakeImageResidentCb = zeContextMakeImageResidentOnExit;
  prologue.Context.pfnEvictImageCb = zeContextEvictImageOnEnter;
  epilogue.Context.pfnEvictImageCb = zeContextEvictImageOnExit;
  prologue.CommandQueue.pfnCreateCb = zeCommandQueueCreateOnEnter;
  epilogue.CommandQueue.pfnCreateCb = zeCommandQueueCreateOnExit;
  prologue.CommandQueue.pfnDestroyCb = zeCommandQueueDestroyOnEnter;
  epilogue.CommandQueue.pfnDestroyCb = zeCommandQueueDestroyOnExit;
  prologue.CommandQueue.pfnExecuteCommandListsCb = zeCommandQueueExecuteCommandListsOnEnter;
  epilogue.CommandQueue.pfnExecuteCommandListsCb = zeCommandQueueExecuteCommandListsOnExit;
  prologue.CommandQueue.pfnSynchronizeCb = zeCommandQueueSynchronizeOnEnter;
  epilogue.CommandQueue.pfnSynchronizeCb = zeCommandQueueSynchronizeOnExit;
  prologue.CommandList.pfnCreateCb = zeCommandListCreateOnEnter;
  epilogue.CommandList.pfnCreateCb = zeCommandListCreateOnExit;
  prologue.CommandList.pfnCreateImmediateCb = zeCommandListCreateImmediateOnEnter;
  epilogue.CommandList.pfnCreateImmediateCb = zeCommandListCreateImmediateOnExit;
  prologue.CommandList.pfnDestroyCb = zeCommandListDestroyOnEnter;
  epilogue.CommandList.pfnDestroyCb = zeCommandListDestroyOnExit;
  prologue.CommandList.pfnCloseCb = zeCommandListCloseOnEnter;
  epilogue.CommandList.pfnCloseCb = zeCommandListCloseOnExit;
  prologue.CommandList.pfnResetCb = zeCommandListResetOnEnter;
  epilogue.CommandList.pfnResetCb = zeCommandListResetOnExit;
  prologue.CommandList.pfnAppendWriteGlobalTimestampCb = zeCommandListAppendWriteGlobalTimestampOnEnter;
  epilogue.CommandList.pfnAppendWriteGlobalTimestampCb = zeCommandListAppendWriteGlobalTimestampOnExit;
  prologue.CommandList.pfnAppendBarrierCb = zeCommandListAppendBarrierOnEnter;
  epilogue.CommandList.pfnAppendBarrierCb = zeCommandListAppendBarrierOnExit;
  prologue.CommandList.pfnAppendMemoryRangesBarrierCb = zeCommandListAppendMemoryRangesBarrierOnEnter;
  epilogue.CommandList.pfnAppendMemoryRangesBarrierCb = zeCommandListAppendMemoryRangesBarrierOnExit;
  prologue.CommandList.pfnAppendMemoryCopyCb = zeCommandListAppendMemoryCopyOnEnter;
  epilogue.CommandList.pfnAppendMemoryCopyCb = zeCommandListAppendMemoryCopyOnExit;
  prologue.CommandList.pfnAppendMemoryFillCb = zeCommandListAppendMemoryFillOnEnter;
  epilogue.CommandList.pfnAppendMemoryFillCb = zeCommandListAppendMemoryFillOnExit;
  prologue.CommandList.pfnAppendMemoryCopyRegionCb = zeCommandListAppendMemoryCopyRegionOnEnter;
  epilogue.CommandList.pfnAppendMemoryCopyRegionCb = zeCommandListAppendMemoryCopyRegionOnExit;
  prologue.CommandList.pfnAppendMemoryCopyFromContextCb = zeCommandListAppendMemoryCopyFromContextOnEnter;
  epilogue.CommandList.pfnAppendMemoryCopyFromContextCb = zeCommandListAppendMemoryCopyFromContextOnExit;
  prologue.CommandList.pfnAppendImageCopyCb = zeCommandListAppendImageCopyOnEnter;
  epilogue.CommandList.pfnAppendImageCopyCb = zeCommandListAppendImageCopyOnExit;
  prologue.CommandList.pfnAppendImageCopyRegionCb = zeCommandListAppendImageCopyRegionOnEnter;
  epilogue.CommandList.pfnAppendImageCopyRegionCb = zeCommandListAppendImageCopyRegionOnExit;
  prologue.CommandList.pfnAppendImageCopyToMemoryCb = zeCommandListAppendImageCopyToMemoryOnEnter;
  epilogue.CommandList.pfnAppendImageCopyToMemoryCb = zeCommandListAppendImageCopyToMemoryOnExit;
  prologue.CommandList.pfnAppendImageCopyFromMemoryCb = zeCommandListAppendImageCopyFromMemoryOnEnter;
  epilogue.CommandList.pfnAppendImageCopyFromMemoryCb = zeCommandListAppendImageCopyFromMemoryOnExit;
  prologue.CommandList.pfnAppendMemoryPrefetchCb = zeCommandListAppendMemoryPrefetchOnEnter;
  epilogue.CommandList.pfnAppendMemoryPrefetchCb = zeCommandListAppendMemoryPrefetchOnExit;
  prologue.CommandList.pfnAppendMemAdviseCb = zeCommandListAppendMemAdviseOnEnter;
  epilogue.CommandList.pfnAppendMemAdviseCb = zeCommandListAppendMemAdviseOnExit;
  prologue.CommandList.pfnAppendSignalEventCb = zeCommandListAppendSignalEventOnEnter;
  epilogue.CommandList.pfnAppendSignalEventCb = zeCommandListAppendSignalEventOnExit;
  prologue.CommandList.pfnAppendWaitOnEventsCb = zeCommandListAppendWaitOnEventsOnEnter;
  epilogue.CommandList.pfnAppendWaitOnEventsCb = zeCommandListAppendWaitOnEventsOnExit;
  prologue.CommandList.pfnAppendEventResetCb = zeCommandListAppendEventResetOnEnter;
  epilogue.CommandList.pfnAppendEventResetCb = zeCommandListAppendEventResetOnExit;
  prologue.CommandList.pfnAppendQueryKernelTimestampsCb = zeCommandListAppendQueryKernelTimestampsOnEnter;
  epilogue.CommandList.pfnAppendQueryKernelTimestampsCb = zeCommandListAppendQueryKernelTimestampsOnExit;
  prologue.CommandList.pfnAppendLaunchKernelCb = zeCommandListAppendLaunchKernelOnEnter;
  epilogue.CommandList.pfnAppendLaunchKernelCb = zeCommandListAppendLaunchKernelOnExit;
  prologue.CommandList.pfnAppendLaunchCooperativeKernelCb = zeCommandListAppendLaunchCooperativeKernelOnEnter;
  epilogue.CommandList.pfnAppendLaunchCooperativeKernelCb = zeCommandListAppendLaunchCooperativeKernelOnExit;
  prologue.CommandList.pfnAppendLaunchKernelIndirectCb = zeCommandListAppendLaunchKernelIndirectOnEnter;
  epilogue.CommandList.pfnAppendLaunchKernelIndirectCb = zeCommandListAppendLaunchKernelIndirectOnExit;
  prologue.CommandList.pfnAppendLaunchMultipleKernelsIndirectCb = zeCommandListAppendLaunchMultipleKernelsIndirectOnEnter;
  epilogue.CommandList.pfnAppendLaunchMultipleKernelsIndirectCb = zeCommandListAppendLaunchMultipleKernelsIndirectOnExit;
  prologue.Fence.pfnCreateCb = zeFenceCreateOnEnter;
  epilogue.Fence.pfnCreateCb = zeFenceCreateOnExit;
  prologue.Fence.pfnDestroyCb = zeFenceDestroyOnEnter;
  epilogue.Fence.pfnDestroyCb = zeFenceDestroyOnExit;
  prologue.Fence.pfnHostSynchronizeCb = zeFenceHostSynchronizeOnEnter;
  epilogue.Fence.pfnHostSynchronizeCb = zeFenceHostSynchronizeOnExit;
  prologue.Fence.pfnQueryStatusCb = zeFenceQueryStatusOnEnter;
  epilogue.Fence.pfnQueryStatusCb = zeFenceQueryStatusOnExit;
  prologue.Fence.pfnResetCb = zeFenceResetOnEnter;
  epilogue.Fence.pfnResetCb = zeFenceResetOnExit;
  prologue.EventPool.pfnCreateCb = zeEventPoolCreateOnEnter;
  epilogue.EventPool.pfnCreateCb = zeEventPoolCreateOnExit;
  prologue.EventPool.pfnDestroyCb = zeEventPoolDestroyOnEnter;
  epilogue.EventPool.pfnDestroyCb = zeEventPoolDestroyOnExit;
  prologue.EventPool.pfnGetIpcHandleCb = zeEventPoolGetIpcHandleOnEnter;
  epilogue.EventPool.pfnGetIpcHandleCb = zeEventPoolGetIpcHandleOnExit;
  prologue.EventPool.pfnOpenIpcHandleCb = zeEventPoolOpenIpcHandleOnEnter;
  epilogue.EventPool.pfnOpenIpcHandleCb = zeEventPoolOpenIpcHandleOnExit;
  prologue.EventPool.pfnCloseIpcHandleCb = zeEventPoolCloseIpcHandleOnEnter;
  epilogue.EventPool.pfnCloseIpcHandleCb = zeEventPoolCloseIpcHandleOnExit;
  prologue.Event.pfnCreateCb = zeEventCreateOnEnter;
  epilogue.Event.pfnCreateCb = zeEventCreateOnExit;
  prologue.Event.pfnDestroyCb = zeEventDestroyOnEnter;
  epilogue.Event.pfnDestroyCb = zeEventDestroyOnExit;
  prologue.Event.pfnHostSignalCb = zeEventHostSignalOnEnter;
  epilogue.Event.pfnHostSignalCb = zeEventHostSignalOnExit;
  prologue.Event.pfnHostSynchronizeCb = zeEventHostSynchronizeOnEnter;
  epilogue.Event.pfnHostSynchronizeCb = zeEventHostSynchronizeOnExit;
  prologue.Event.pfnQueryStatusCb = zeEventQueryStatusOnEnter;
  epilogue.Event.pfnQueryStatusCb = zeEventQueryStatusOnExit;
  prologue.Event.pfnHostResetCb = zeEventHostResetOnEnter;
  epilogue.Event.pfnHostResetCb = zeEventHostResetOnExit;
  prologue.Event.pfnQueryKernelTimestampCb = zeEventQueryKernelTimestampOnEnter;
  epilogue.Event.pfnQueryKernelTimestampCb = zeEventQueryKernelTimestampOnExit;
  prologue.Image.pfnGetPropertiesCb = zeImageGetPropertiesOnEnter;
  epilogue.Image.pfnGetPropertiesCb = zeImageGetPropertiesOnExit;
  prologue.Image.pfnCreateCb = zeImageCreateOnEnter;
  epilogue.Image.pfnCreateCb = zeImageCreateOnExit;
  prologue.Image.pfnDestroyCb = zeImageDestroyOnEnter;
  epilogue.Image.pfnDestroyCb = zeImageDestroyOnExit;
  prologue.Module.pfnCreateCb = zeModuleCreateOnEnter;
  epilogue.Module.pfnCreateCb = zeModuleCreateOnExit;
  prologue.Module.pfnDestroyCb = zeModuleDestroyOnEnter;
  epilogue.Module.pfnDestroyCb = zeModuleDestroyOnExit;
  prologue.Module.pfnDynamicLinkCb = zeModuleDynamicLinkOnEnter;
  epilogue.Module.pfnDynamicLinkCb = zeModuleDynamicLinkOnExit;
  prologue.Module.pfnGetNativeBinaryCb = zeModuleGetNativeBinaryOnEnter;
  epilogue.Module.pfnGetNativeBinaryCb = zeModuleGetNativeBinaryOnExit;
  prologue.Module.pfnGetGlobalPointerCb = zeModuleGetGlobalPointerOnEnter;
  epilogue.Module.pfnGetGlobalPointerCb = zeModuleGetGlobalPointerOnExit;
  prologue.Module.pfnGetKernelNamesCb = zeModuleGetKernelNamesOnEnter;
  epilogue.Module.pfnGetKernelNamesCb = zeModuleGetKernelNamesOnExit;
  prologue.Module.pfnGetPropertiesCb = zeModuleGetPropertiesOnEnter;
  epilogue.Module.pfnGetPropertiesCb = zeModuleGetPropertiesOnExit;
  prologue.Module.pfnGetFunctionPointerCb = zeModuleGetFunctionPointerOnEnter;
  epilogue.Module.pfnGetFunctionPointerCb = zeModuleGetFunctionPointerOnExit;
  prologue.ModuleBuildLog.pfnDestroyCb = zeModuleBuildLogDestroyOnEnter;
  epilogue.ModuleBuildLog.pfnDestroyCb = zeModuleBuildLogDestroyOnExit;
  prologue.ModuleBuildLog.pfnGetStringCb = zeModuleBuildLogGetStringOnEnter;
  epilogue.ModuleBuildLog.pfnGetStringCb = zeModuleBuildLogGetStringOnExit;
  prologue.Kernel.pfnCreateCb = zeKernelCreateOnEnter;
  epilogue.Kernel.pfnCreateCb = zeKernelCreateOnExit;
  prologue.Kernel.pfnDestroyCb = zeKernelDestroyOnEnter;
  epilogue.Kernel.pfnDestroyCb = zeKernelDestroyOnExit;
  prologue.Kernel.pfnSetCacheConfigCb = zeKernelSetCacheConfigOnEnter;
  epilogue.Kernel.pfnSetCacheConfigCb = zeKernelSetCacheConfigOnExit;
  prologue.Kernel.pfnSetGroupSizeCb = zeKernelSetGroupSizeOnEnter;
  epilogue.Kernel.pfnSetGroupSizeCb = zeKernelSetGroupSizeOnExit;
  prologue.Kernel.pfnSuggestGroupSizeCb = zeKernelSuggestGroupSizeOnEnter;
  epilogue.Kernel.pfnSuggestGroupSizeCb = zeKernelSuggestGroupSizeOnExit;
  prologue.Kernel.pfnSuggestMaxCooperativeGroupCountCb = zeKernelSuggestMaxCooperativeGroupCountOnEnter;
  epilogue.Kernel.pfnSuggestMaxCooperativeGroupCountCb = zeKernelSuggestMaxCooperativeGroupCountOnExit;
  prologue.Kernel.pfnSetArgumentValueCb = zeKernelSetArgumentValueOnEnter;
  epilogue.Kernel.pfnSetArgumentValueCb = zeKernelSetArgumentValueOnExit;
  prologue.Kernel.pfnSetIndirectAccessCb = zeKernelSetIndirectAccessOnEnter;
  epilogue.Kernel.pfnSetIndirectAccessCb = zeKernelSetIndirectAccessOnExit;
  prologue.Kernel.pfnGetIndirectAccessCb = zeKernelGetIndirectAccessOnEnter;
  epilogue.Kernel.pfnGetIndirectAccessCb = zeKernelGetIndirectAccessOnExit;
  prologue.Kernel.pfnGetSourceAttributesCb = zeKernelGetSourceAttributesOnEnter;
  epilogue.Kernel.pfnGetSourceAttributesCb = zeKernelGetSourceAttributesOnExit;
  prologue.Kernel.pfnGetPropertiesCb = zeKernelGetPropertiesOnEnter;
  epilogue.Kernel.pfnGetPropertiesCb = zeKernelGetPropertiesOnExit;
  prologue.Kernel.pfnGetNameCb = zeKernelGetNameOnEnter;
  epilogue.Kernel.pfnGetNameCb = zeKernelGetNameOnExit;
  prologue.Sampler.pfnCreateCb = zeSamplerCreateOnEnter;
  epilogue.Sampler.pfnCreateCb = zeSamplerCreateOnExit;
  prologue.Sampler.pfnDestroyCb = zeSamplerDestroyOnEnter;
  epilogue.Sampler.pfnDestroyCb = zeSamplerDestroyOnExit;
  prologue.PhysicalMem.pfnCreateCb = zePhysicalMemCreateOnEnter;
  epilogue.PhysicalMem.pfnCreateCb = zePhysicalMemCreateOnExit;
  prologue.PhysicalMem.pfnDestroyCb = zePhysicalMemDestroyOnEnter;
  epilogue.PhysicalMem.pfnDestroyCb = zePhysicalMemDestroyOnExit;
  prologue.Mem.pfnAllocSharedCb = zeMemAllocSharedOnEnter;
  epilogue.Mem.pfnAllocSharedCb = zeMemAllocSharedOnExit;
  prologue.Mem.pfnAllocDeviceCb = zeMemAllocDeviceOnEnter;
  epilogue.Mem.pfnAllocDeviceCb = zeMemAllocDeviceOnExit;
  prologue.Mem.pfnAllocHostCb = zeMemAllocHostOnEnter;
  epilogue.Mem.pfnAllocHostCb = zeMemAllocHostOnExit;
  prologue.Mem.pfnFreeCb = zeMemFreeOnEnter;
  epilogue.Mem.pfnFreeCb = zeMemFreeOnExit;
  prologue.Mem.pfnGetAllocPropertiesCb = zeMemGetAllocPropertiesOnEnter;
  epilogue.Mem.pfnGetAllocPropertiesCb = zeMemGetAllocPropertiesOnExit;
  prologue.Mem.pfnGetAddressRangeCb = zeMemGetAddressRangeOnEnter;
  epilogue.Mem.pfnGetAddressRangeCb = zeMemGetAddressRangeOnExit;
  prologue.Mem.pfnGetIpcHandleCb = zeMemGetIpcHandleOnEnter;
  epilogue.Mem.pfnGetIpcHandleCb = zeMemGetIpcHandleOnExit;
  prologue.Mem.pfnOpenIpcHandleCb = zeMemOpenIpcHandleOnEnter;
  epilogue.Mem.pfnOpenIpcHandleCb = zeMemOpenIpcHandleOnExit;
  prologue.Mem.pfnCloseIpcHandleCb = zeMemCloseIpcHandleOnEnter;
  epilogue.Mem.pfnCloseIpcHandleCb = zeMemCloseIpcHandleOnExit;
  prologue.VirtualMem.pfnReserveCb = zeVirtualMemReserveOnEnter;
  epilogue.VirtualMem.pfnReserveCb = zeVirtualMemReserveOnExit;
  prologue.VirtualMem.pfnFreeCb = zeVirtualMemFreeOnEnter;
  epilogue.VirtualMem.pfnFreeCb = zeVirtualMemFreeOnExit;
  prologue.VirtualMem.pfnQueryPageSizeCb = zeVirtualMemQueryPageSizeOnEnter;
  epilogue.VirtualMem.pfnQueryPageSizeCb = zeVirtualMemQueryPageSizeOnExit;
  prologue.VirtualMem.pfnMapCb = zeVirtualMemMapOnEnter;
  epilogue.VirtualMem.pfnMapCb = zeVirtualMemMapOnExit;
  prologue.VirtualMem.pfnUnmapCb = zeVirtualMemUnmapOnEnter;
  epilogue.VirtualMem.pfnUnmapCb = zeVirtualMemUnmapOnExit;
  prologue.VirtualMem.pfnSetAccessAttributeCb = zeVirtualMemSetAccessAttributeOnEnter;
  epilogue.VirtualMem.pfnSetAccessAttributeCb = zeVirtualMemSetAccessAttributeOnExit;
  prologue.VirtualMem.pfnGetAccessAttributeCb = zeVirtualMemGetAccessAttributeOnEnter;
  epilogue.VirtualMem.pfnGetAccessAttributeCb = zeVirtualMemGetAccessAttributeOnExit;

  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zelTracerSetPrologues(tracer, &prologue);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
  status = zelTracerSetEpilogues(tracer, &epilogue);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);
}

#endif // PTI_SAMPLES_ZE_HOT_FUNCTIONS_ZE_API_CALLBACKS_H_
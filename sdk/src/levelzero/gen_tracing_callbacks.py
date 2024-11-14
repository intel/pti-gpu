# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import re

# Parse Level Zero Headers ####################################################
# Extract registratable apis from 2 headers (ze_api.h, layers/zel_tracing_register_cb.h)
# Generate a tracing.gen file to be included into ze_collector.h that can be used to register callbacks.

STATE_NORMAL = 0
STATE_CONDITION = 1
STATE_SKIP = 2
FILE_OPEN_PERMISSIONS = 0o600


# https://docs.python.org/3/library/functions.html#open
def default_file_opener(path, flags):
    return os.open(path, flags, mode=FILE_OPEN_PERMISSIONS)


# Generates a func_name -> <name of matching param structure> mapping for all L0 api registrable functions.
# returns a dict of this to caller.
# e.g entry in dict:  'zeCommandListAppendLaunchKernel': 'ze_command_list_append_launch_kernel_params_t* '


def gen_func_param_dict(api_files):
    func_dict = {}
    for api_filepath in api_files:
        with open(api_filepath, "rt", opener=default_file_opener) as f:
            f.seek(0)
            # for line in f.readlines():
            for line in f:
                if line.find("ZE_APICALL") != -1 and line.find("ze_pfn") != -1:
                    items = line.split("ze_pfn")
                    assert len(items) == 2
                    assert items[1].find("Cb_t") != -1
                    items = items[1].split("Cb_t")
                    assert len(items) == 2
                    func_dict["ze" + items[0].strip()] = next(f).strip()
                    # func_list.append("ze" + items[0].strip())
                    # print("FUNC_LIST -- next line: ",next(f).split("* params,")[0])
    return func_dict


#
# Generates Individual Type Related function Id files
# Generates map objects for those ids into the main generated callback api file
#
def gen_cbid_files_per_category(dst_path, cb_api_file, category_dict):
    for category, callback_list in category_dict.items():
        cbids_file_path = os.path.join(dst_path, "pti_cbids_" + category + ".h")
        with open(cbids_file_path, "wt", opener=default_file_opener) as f:
            f.write(
                "//==============================================================\n"
            )
            f.write("// Copyright (C) Intel Corporation\n")
            f.write("//\n")
            f.write("// SPDX-License-Identifier: MIT\n")
            f.write(
                "// =============================================================\n"
            )
            f.write("\n")
            f.write("#ifndef PTI_CB_API_ID_" + category.upper() + "_H\n")
            f.write("#define PTI_CB_API_ID_" + category.upper() + "_H\n")
            f.write("\n")
            f.write(
                "// ========= This file is autogenerated - do not modify ========\n"
            )
            f.write("\n\n")
            f.write(" typedef enum _pti_callback_api_id_" + category + " {\n")
            id_counter = 0
            f.write("    reserved_" + category + "_id=" + str(id_counter) + ",\n")
            for func in callback_list:
                id_counter += 1
                f.write("    " + func + "_id=" + str(id_counter) + ",\n")
            # f.write("    last_"+category+"_id=" + str(id_counter) +",\n")
            f.write(" } pti_callback_api_id_" + category + ";\n")
            f.write("\n")
            f.write("#endif\n")

            cb_api_file.write(
                "inline static std::map<uint32_t, const char*> pti_callback_api_id_"
                + category
                + "_cb_name =\n"
            )
            cb_api_file.write("  {\n")
            for func in callback_list:
                cb_api_file.write(
                    "    {pti_callback_api_id_"
                    + category
                    + "::"
                    + func
                    + '_id, "'
                    + func
                    + '"},\n'
                )
            cb_api_file.write("  }; \n\n")


# Generates EnableTracing function and populates with registrable signatures for all apis: partitioned by all or just kfunc.
def gen_api(
    f, func_list, kfunc_list, exclude_from_epilogue_list, exclude_from_prologue_list
):
    f.write("void EnableTracer(zel_tracer_handle_t tracer) {\n")
    f.write("\n")
    f.write("  if (options_.api_tracing) {\n")
    for func in func_list:
        f.write(
            "    zelTracer"
            + func[2:]
            + "RegisterCallback("
            + "tracer, ZEL_REGISTER_PROLOGUE, "
            + func
            + "OnEnter);\n"
        )
        f.write(
            "    zelTracer"
            + func[2:]
            + "RegisterCallback("
            + "tracer, ZEL_REGISTER_EPILOGUE, "
            + func
            + "OnExit);\n"
        )
    f.write("  }\n")

    f.write("  else if (options_.kernel_tracing) {\n")
    for func in kfunc_list:
        if func not in exclude_from_prologue_list:
            f.write(
                "    zelTracer"
                + func[2:]
                + "RegisterCallback("
                + "tracer, ZEL_REGISTER_PROLOGUE, "
                + func
                + "OnEnter);\n"
            )
        if func not in exclude_from_epilogue_list:
            f.write(
                "    zelTracer"
                + func[2:]
                + "RegisterCallback("
                + "tracer, ZEL_REGISTER_EPILOGUE, "
                + func
                + "OnExit);\n"
            )
    f.write("  }\n")

    f.write("\n")
    f.write("  ze_result_t status = ZE_RESULT_SUCCESS;\n")
    f.write("\n")
    f.write("  overhead::Init();\n")
    f.write("  status = zelTracerSetEnabled(tracer, true);\n")
    f.write("  overhead_fini(zelTracerSetEnabled_id);\n")
    f.write("  PTI_ASSERT(status == ZE_RESULT_SUCCESS);\n")
    f.write("}\n")
    f.write("\n")


# TODO -- is this needed?
def gen_structure_type_converter(f, enum_map):
    struct_type_enum = {}
    for name in enum_map["ze_structure_type_t"]:
        struct_type_enum[name] = int(enum_map["ze_structure_type_t"][name])
    struct_type_enum = sorted(struct_type_enum.items(), key=lambda x: x[1])
    assert "ze_structure_type_t" in enum_map
    f.write("static const char* GetStructureTypeString(unsigned structure_type) {\n")
    f.write("  switch (structure_type) {\n")
    for name, value in struct_type_enum:
        f.write("    case " + name + ":\n")
        f.write('      return "' + name + '";\n')
    f.write("    default:\n")
    f.write("      break;\n")
    f.write("  }\n")
    f.write('  return "UNKNOWN";\n')
    f.write("}\n")
    f.write("\n")


# TODO -- is this needed?
def gen_result_converter(f, enum_map):
    result_enum = {}
    for name in enum_map["ze_result_t"]:
        result_enum[name] = int(enum_map["ze_result_t"][name])
    result_enum = sorted(result_enum.items(), key=lambda x: x[1])
    assert "ze_result_t" in enum_map
    f.write("static const char* GetResultString(unsigned result) {\n")
    f.write("  switch (result) {\n")
    for name, value in result_enum:
        f.write("    case " + name + ":\n")
        f.write('      return "' + name + '";\n')
    f.write("    default:\n")
    f.write("      break;\n")
    f.write("  }\n")
    f.write('  return "UNKNOWN";\n')
    f.write("}\n")
    f.write("\n")


# Checks if forwarding callback exists in ze_collector.h -- used to check before the stub issues the forwarding call.
def get_kernel_tracing_callback(func):
    d = os.path.dirname(os.path.abspath(__file__))
    cb = ""
    with open(os.path.join(d, "ze_collector.h"), opener=default_file_opener) as fp:
        if func in fp.read():
            cb = func
        fp.close()
        return cb


# Generate the OnEnter stub and issue forwarding call if cb exists in ze_collector.h
def gen_enter_callback(f, func, synchronize_func_list_on_enter, hybrid_mode_func_list):
    f.write("  [[maybe_unused]] ZeCollector* collector =\n")
    f.write("    static_cast<ZeCollector*>(global_data);\n")
    if func not in hybrid_mode_func_list:
        f.write("  if (collector->options_.hybrid_mode) return;\n")

    if func in synchronize_func_list_on_enter:
        f.write("  std::vector<uint64_t> kids;\n")

    f.write("\n")
    cb = get_kernel_tracing_callback("OnEnter" + func[2:])
    if cb != "":
        f.write("  if (collector->options_.kernel_tracing) { \n")
        if func in synchronize_func_list_on_enter:
            f.write(
                "    " + cb + "(params, global_data, instance_user_data, &kids); \n"
            )
            f.write("    if (kids.size() != 0) {\n")
            f.write(
                "        ze_instance_data.kid = kids[0];\n"
            )  # pass kid to the exit callback
            f.write("    }\n")
            f.write("    else {\n")
            f.write("        ze_instance_data.kid = (uint64_t)(-1);\n")
            f.write("    }\n")
        else:
            f.write("    " + cb + "(params, global_data, instance_user_data); \n")
        f.write("  }\n")
    f.write("\n")

    f.write("  uint64_t start_time_host = 0;\n")
    f.write("  start_time_host = utils::GetTime();\n")

    f.write("  ze_instance_data.start_time_host = start_time_host;\n")


# Generate the OnExit stub and issue forwarding call if cb exists in ze_collector.h
def gen_exit_callback(
    f,
    func,
    submission_func_list,
    synchronize_func_list_on_enter,
    synchronize_func_list_on_exit,
    hybrid_mode_func_list,
    km_rt_func_list,
    ze_gen_func_list,
):
    f.write("  [[maybe_unused]] ZeCollector* collector =\n")
    f.write("    static_cast<ZeCollector*>(global_data);\n")
    if func not in hybrid_mode_func_list:
        f.write("  if (collector->options_.hybrid_mode) return;\n")

    f.write("  [[maybe_unused]] uint64_t end_time_host = 0;\n")
    f.write("  end_time_host = utils::GetTime();\n")

    cb = get_kernel_tracing_callback("OnExit" + func[2:])

    if (
        (func in submission_func_list)
        or (func in synchronize_func_list_on_enter)
        or (func in synchronize_func_list_on_exit)
    ):
        f.write("  std::vector<uint64_t> kids;\n")

    if (
        func in synchronize_func_list_on_enter
    ):  # enter callback pass the kids to exit callback
        f.write("  if (ze_instance_data.kid != (uint64_t)(-1)) {\n")
        f.write("      kids.push_back(ze_instance_data.kid);\n")
        f.write("  }\n")

    if cb != "":
        f.write("  if (collector->options_.kernel_tracing) { \n")
        if (func in submission_func_list) or (func in synchronize_func_list_on_exit):
            f.write(
                "    "
                + cb
                + "(params, result, global_data, instance_user_data, &kids); \n"
            )
        else:
            f.write(
                "    " + cb + "(params, result, global_data, instance_user_data); \n"
            )
        f.write("  }\n")

        f.write("\n")

    f.write("\n")
    f.write("  uint64_t start_time_host = ze_instance_data.start_time_host;\n")
    f.write("\n")
    f.write("  if (start_time_host == 0) {\n")
    f.write("    return;\n")
    f.write("  }\n")

    if func in ze_gen_func_list:
        f.write(
            "  ZeKernelCommand *kc = static_cast<ZeKernelCommand*>(*instance_user_data);\n"
        )
        f.write("  kc->callback_id_ = zeCommandListAppendLaunchKernel_id;\n")
        f.write("  kc->api_start_time_ = start_time_host;\n")
        f.write("  kc->api_end_time_ = end_time_host;\n")
    f.write(
        "  if (collector->cb_enabled_.fcallback && collector->fcallback_ != nullptr) {\n"
    )
    f.write("    ZeKernelCommandExecutionRecord rec = {};\n")
    f.write("    rec.start_time_ = start_time_host;\n")
    f.write("    rec.end_time_ = end_time_host;\n")
    f.write("    rec.callback_id_ = " + func + "_id;\n")
    if func in km_rt_func_list:
        f.write("    if (sycl_data_kview.cid_) {\n")
        f.write("      rec.cid_ = sycl_data_kview.cid_;\n")
        f.write("    } else if (sycl_data_mview.cid_) {\n")
        f.write("      rec.cid_ = sycl_data_mview.cid_;\n")
        f.write("    } else {\n")
        f.write("      rec.cid_ = UniCorrId::GetUniCorrId();\n")
        f.write("    }\n")
    else:
        f.write("    rec.cid_ = UniCorrId::GetUniCorrId();\n")
    if func in km_rt_func_list:
        f.write("    sycl_data_kview.cid_ = 0;\n")
        f.write("    sycl_data_mview.cid_ = 0;\n")
    # f.write("    sycl_data_kview.cid_ = 0;\n")
    # f.write("    sycl_data_mview.cid_ = 0;\n")
    f.write("    rec.pid_ = thread_local_pid_tid_info.pid;\n")
    f.write("    rec.tid_ = thread_local_pid_tid_info.tid;\n")
    f.write("    rec.result_ = result;\n")
    f.write("    collector->fcallback_(collector->callback_data_, rec);\n")
    f.write("  }\n")
    f.write("\n")
    f.write("\n")


# Generate OnEnter and OnExit callbacks.
def gen_callbacks(
    f,
    func_param_dict,
    submission_func_list,
    synchronize_func_list_on_enter,
    synchronize_func_list_on_exit,
    hybrid_mode_func_list,
    km_rt_func_list,
    ze_gen_func_list,
):
    for func in func_param_dict.keys():
        # print ("+++ Function : ", func)
        f.write("static void " + func + "OnEnter(\n")
        f.write("    [[maybe_unused]]" + func_param_dict[func] + "\n")
        f.write("    [[maybe_unused]]ze_result_t result,\n")
        f.write("    [[maybe_unused]]void* global_data,\n")
        f.write("    [[maybe_unused]]void** instance_user_data) {\n")
        gen_enter_callback(
            f, func, synchronize_func_list_on_enter, hybrid_mode_func_list
        )
        f.write("}\n")
        f.write("\n")
        f.write("static void " + func + "OnExit(\n")
        f.write("    " + "[[maybe_unused]]" + func_param_dict[func] + "\n")
        f.write("    [[maybe_unused]]ze_result_t result,\n")
        f.write("    [[maybe_unused]]void* global_data,\n")
        f.write("    [[maybe_unused]]void** instance_user_data) {\n")
        gen_exit_callback(
            f,
            func,
            submission_func_list,
            synchronize_func_list_on_enter,
            synchronize_func_list_on_exit,
            hybrid_mode_func_list,
            km_rt_func_list,
            ze_gen_func_list,
        )
        f.write("}\n")
        f.write("\n")


def append_undefined_functions(func_list, func_dictionary):
    """
    in Level-Zero 14 that PTI uses to build - these functions not defined yet.
    However for Local profiling PTI relies that these functiond are present in the
    L0 installed on the system.
    So we add these functions to the list of functions that will be used to generate CBids
    """
    additional_functions = [
        "zeEventPoolGetFlags",
        "zeCommandListGetDeviceHandle",
        "zeCommandListGetContextHandle",
        "zeCommandListIsImmediate",
        "zeCommandListImmediateGetIndex",
        "zeCommandListGetOrdinal",
        "zeCommandQueueGetIndex",
        "zeCommandQueueGetOrdinal",
    ]

    tracing_functions = [
        "zelTracerSetEnabled",
        "zelTracerCreate",
    ]

    for func in additional_functions:
        if func not in func_dictionary.keys():
            func_list.append(func)

    for func in tracing_functions:
        func_list.append(func)


def main():
    if len(sys.argv) < 3:
        print(
            "Usage: python gen_tracing_header.py <output_include_path> <l0_include_path>"
        )
        return

    print("Using: python gen_tracing_header.py", sys.argv[1], sys.argv[2])
    dst_path = sys.argv[1]
    if not os.path.exists(dst_path):
        os.mkdir(dst_path)

    dst_file_path = os.path.join(dst_path, "tracing.gen")
    if os.path.isfile(dst_file_path):
        os.remove(dst_file_path)

    dst_cb_api_file_path = os.path.join(dst_path, "tracing_cb_api.gen")
    if os.path.isfile(dst_cb_api_file_path):
        os.remove(dst_cb_api_file_path)

    dst_file = open(dst_file_path, "wt", opener=default_file_opener)
    dst_cb_api_file = open(dst_cb_api_file_path, "wt", opener=default_file_opener)

    l0_path = sys.argv[2]

    # Extract registratable apis from 2 headers (ze_api.h, layers/zel_tracing_register_cb.h)
    # Both are complementary to each other. One addresses apis from v1.0(ze_api.h) the other v1.1+.
    l0_file_path_api_1_0 = os.path.join(l0_path, "ze_api.h")
    l0_file_path_api_1_1_plus = os.path.join(
        l0_path, "layers", "zel_tracing_register_cb.h"
    )
    proj_bin_path = sys.argv[3]

    l0_api_files = [l0_file_path_api_1_0, l0_file_path_api_1_1_plus]
    func_param_dictionary = gen_func_param_dict(l0_api_files)
    func_list = list(func_param_dictionary.keys())

    kfunc_list = [
        "zeEventDestroy",
        "zeEventHostReset",
        "zeEventPoolCreate",
        "zeCommandListAppendLaunchKernel",
        "zeCommandListAppendLaunchCooperativeKernel",
        "zeCommandListAppendLaunchKernelIndirect",
        "zeCommandListAppendMemoryCopy",
        "zeCommandListAppendMemoryFill",
        "zeCommandListAppendBarrier",
        "zeCommandListAppendMemoryRangesBarrier",
        "zeCommandListAppendMemoryCopyRegion",
        "zeCommandListAppendMemoryCopyFromContext",
        "zeCommandListAppendImageCopy",
        "zeCommandListAppendImageCopyRegion",
        "zeCommandListAppendImageCopyToMemory",
        "zeCommandListAppendImageCopyFromMemory",
        "zeCommandQueueExecuteCommandLists",
        "zeCommandListCreate",
        "zeCommandListCreateImmediate",
        "zeCommandListDestroy",
        "zeCommandListReset",
        "zeCommandQueueCreate",
        "zeCommandQueueSynchronize",
        "zeCommandQueueDestroy",
        "zeImageCreate",
        "zeImageDestroy",
        "zeKernelSetGroupSize",
        "zeKernelDestroy",
        "zeEventHostSynchronize",
        "zeFenceHostSynchronize",
        "zeEventQueryStatus",
        "zeCommandListHostSynchronize",
        "zeContextDestroy",
    ]

    ze_gen_func_list = [
        "zeCommandListAppendLaunchKernel",
    ]

    km_rt_func_list = [
        "zeCommandListAppendLaunchKernel",
        "zeCommandListAppendLaunchCooperativeKernel",
        "zeCommandListAppendLaunchKernelIndirect",
        "zeCommandListAppendMemoryCopy",
        "zeCommandListAppendMemoryFill",
        "zeCommandListAppendMemoryCopyRegion",
        "zeCommandListAppendMemoryCopyFromContext",
        "zeCommandListAppendImageCopy",
        "zeCommandListAppendImageCopyRegion",
        "zeCommandListAppendImageCopyToMemory",
        "zeCommandListAppendImageCopyFromMemory",
    ]

    hybrid_mode_func_list = ["zeEventPoolCreate"]

    submission_func_list = [
        "zeCommandQueueExecuteCommandLists",
        "zeCommandListAppendLaunchKernel",
        "zeCommandListAppendLaunchCooperativeKernel",
        "zeCommandListAppendLaunchKernelIndirect",
        "zeCommandListAppendMemoryCopy",
        "zeCommandListAppendMemoryFill",
        "zeCommandListAppendBarrier",
        "zeCommandListAppendMemoryRangesBarrier",
        "zeCommandListAppendMemoryCopyRegion",
        "zeCommandListAppendMemoryCopyFromContext",
        "zeCommandListAppendImageCopy",
        "zeCommandListAppendImageCopyRegion",
        "zeCommandListAppendImageCopyToMemory",
        "zeCommandListAppendImageCopyFromMemory",
    ]

    synchronize_func_list_on_enter = ["zeEventDestroy", "zeEventHostReset"]

    synchronize_func_list_on_exit = [
        "zeCommandListHostSynchronize",
        "zeEventHostSynchronize",
        "zeEventQueryStatus",
        "zeFenceHostSynchronize",
        "zeCommandQueueSynchronize",
    ]

    exclude_from_epilogue_list = []

    exclude_from_prologue_list = ["zeCommandListHostSynchronize"]

    #######################################################
    # Callbacks API domain/category lists with apis within
    # Max 64 categories can be supported
    #######################################################

    runtime_list = [
        "zeCommandListAppendLaunchKernel",
    ]

    resource_list = []

    # category_dict = {"runtime": func_list, "resource": resource_list}
    category_dict = {"runtime": func_list}

    gen_callbacks(
        dst_file,
        func_param_dictionary,
        submission_func_list,
        synchronize_func_list_on_enter,
        synchronize_func_list_on_exit,
        hybrid_mode_func_list,
        km_rt_func_list,
        ze_gen_func_list,
    )
    gen_api(
        dst_file,
        func_list,
        kfunc_list,
        exclude_from_epilogue_list,
        exclude_from_prologue_list,
    )

    # add functions that might be undefined in the L0 header used to build PTI
    # they will  be checked in runtime .. and used if found
    # here we need to generate CBids for them - that are used in the Overhead collection
    append_undefined_functions(category_dict["runtime"], func_param_dictionary)

    gen_cbid_files_per_category(proj_bin_path, dst_cb_api_file, category_dict)

    dst_file.close()


if __name__ == "__main__":
    main()

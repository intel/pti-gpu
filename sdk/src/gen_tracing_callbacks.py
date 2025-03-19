# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

import os
import sys
import re
import pprint
import bisect

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


#
# Extracts api function names from tracing_types.h and returns as list.
#
def get_ocl_api_list(ocl_path):
    ocl_file_path = os.path.join(ocl_path, "tracing_types.h")
    ocl_file = open(ocl_file_path, "rt")
    func_list = []
    cl_function_id_found = False
    for line in ocl_file.readlines():
        line = line.strip()
        # print(line)
        if cl_function_id_found == True and line.find("CL_FUNCTION_COUNT") == -1:
            if line.startswith("CL_FUNCTION_"):
                api_name = line[
                    len("CL_FUNCTION_") : line.find("=", len("CL_FUNCTION_"))
                ]
                func_list.append(api_name.strip())
        elif line.find("ClFunctionId") != -1:
            cl_function_id_found = True
    return func_list


#
# Extracts api function names from ur_api.h and returns as list.
# Note:  The ur_api.h contains predefined ids for each api -- assumption is no recycle on deprecation.
#
def get_ur_api_list(ur_file_path):
    ur_file = open(ur_file_path, "rt")
    func_list = []
    ur_function_id_found = False
    for line in ur_file.readlines():
        line = line.strip()
        # print(line)
        if line.find("version v") != -1:
            ur_api_version = line
        if ur_function_id_found == True and line.find("UR_FUNCTION_FORCE_UINT32") == -1:
            if line.startswith("UR_FUNCTION_"):
                api_id = line[line.find("=") + 1 : line.find(",")].strip()
                api_name = line[
                    line.find("Enumerator for ::") + len("Enumerator for ::") :
                ]
                func_list.append(api_name.strip() + "," + api_id)
        elif line.find("ur_function_t") != -1:
            ur_function_id_found = True
    return (func_list, ur_api_version)


#
# Generates a api func_name -> <exposed apiid> mapping for all L0 api registrable functions.
# returns a dict of this to caller.
#
def get_apiids_per_category(src_path, category_dict):
    apiids_dict_per_category = {}
    # for category, callback_list in category_dict.items():
    for cat_key, callback_list in category_dict.items():
        category = cat_key[0]
        api_group = cat_key[1]
        print("getting existing: working with: ", category, api_group)
        apiids_file_path = os.path.join(
            src_path, "pti_" + category + "_" + api_group + "_api_ids.h"
        )
        if os.path.exists(apiids_file_path):
            apiids_dict = {}
            apiids_set = set()
            apiids_counter = 0
            with open(apiids_file_path, "rt", opener=default_file_opener) as f:
                f.seek(0)
                reserved_found = False
                id = 0
                for line in f:
                    if line.find("reserved_") != -1:
                        reserved_found = True
                    elif line.find("pti_api_id_") != -1:
                        reserved_found = False
                    elif reserved_found and (
                        (line.strip().startswith("ze") != -1)
                        or (line.strip().startswith("cl") != -1)
                    ):
                        items = line.split("=")
                        assert len(items) == 2
                        id = items[1].split(",")
                        apiids_dict[items[0].strip()] = id[0]
                        apiids_set.add(int(id[0]))
                        apiids_dict["next_elem_index"] = max(apiids_set) + 1
                        apiids_counter += 1
                        # print(items, apiids_counter, id[0], len(apiids_set))
                        assert apiids_counter == len(
                            apiids_set
                        ), "Duplicated api_id found in existing"

            # print(apiids_dict["next_elem_index"])
            apiids_dict_per_category[cat_key] = apiids_dict.copy()
    return apiids_dict_per_category


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
                if line.find("version v") != -1:
                    L0_api_version = line
                    print(line)
                if line.find("ZE_APICALL") != -1 and line.find("ze_pfn") != -1:
                    items = line.split("ze_pfn")
                    assert len(items) == 2
                    assert items[1].find("Cb_t") != -1
                    items = items[1].split("Cb_t")
                    assert len(items) == 2
                    func_dict["ze" + items[0].strip()] = next(f).strip()
                    # func_list.append("ze" + items[0].strip())
                    # print("FUNC_LIST -- next line: ",next(f).split("* params,")[0])
    return (func_dict, L0_api_version)


def gen_apiid_header(api_file, header_string, api_version_info):
    api_file.write("//==============================================================\n")
    api_file.write("// Copyright (C) Intel Corporation\n")
    api_file.write("//\n")
    api_file.write("// SPDX-License-Identifier: MIT\n")
    api_file.write("// =============================================================\n")
    api_file.write("\n")
    api_file.write("#ifndef PTI_API_ID_" + header_string + "_H_\n")
    api_file.write("#define PTI_API_ID_" + header_string + "_H_\n")
    api_file.write("\n")
    api_file.write("// ========= This file is autogenerated - do not modify ========\n")
    if api_version_info:
        api_file.write(
            "// ========= Api file version used for generation: =============\n"
        )
        api_file.write("//    ApiFile: " + api_version_info[1] + "\n")
        api_file.write("// ApiVersion:" + api_version_info[0] + "\n")
        api_file.write("//             " + api_version_info[2] + "\n")
        api_file.write("//             " + api_version_info[3] + "\n")
    api_file.write("\n\n")


def gen_apiid_terminate_header(api_file):
    api_file.write("#endif\n")


#
def generate_api_file_portion(
    apiids_file_path,
    callback_list,
    category,
    api_group,
    existing_apiids,
    dst_validation_file,
    version_dict,
):
    kApiIdEnumInterval = 1
    with open(apiids_file_path, "wt", opener=default_file_opener) as f:
        gen_apiid_header(
            f, category.upper() + "_" + api_group.upper(), version_dict[api_group]
        )
        f.write("typedef enum _pti_api_id_" + category + "_" + api_group + " {\n")
        f.write("    reserved_" + category + "_" + api_group + "_id=" + str(0) + ",\n")
        next_id = int(existing_apiids.get("next_elem_index", kApiIdEnumInterval))
        if existing_apiids:
            del existing_apiids["next_elem_index"]
        id_counter = 0
        id_set = set()
        # clist = sorted(callback_list, key=lambda x: x[0])
        sorted_api_list = []
        unknown_sorted_api_list = []
        for func in callback_list:
            items = func.partition(",")
            # print(items)
            apiid = int(existing_apiids.get(items[0] + "_id", 0))
            # Use existing apiid or use one in the header or create one if not known
            # Is this header api already known to ptisdk
            known_api = True
            if apiid == 0:  # not known, no existing id
                known_api = False
                if (
                    items[2] == ""
                ):  # and no default value in the header file, create one.
                    apiid = next_id
                    if bool(id_set) and (
                        next_id < max(id_set)
                    ):  # set is not empty and current next_id is lesser.
                        apiid = max(id_set) + 1
                    else:
                        next_id += 1
                    # print("Apidd: ", apiid)
                else:  # there is a default value in the header -- use it.
                    apiid = int(items[2])
                    # print("items2: ", type(apiid))
            else:
                del existing_apiids[items[0] + "_id"]
            # f.write("    " + items[0] + "_id=" + str(apiid) + ",\n")
            # if (not known_api):
            #    bisect.insort(unknown_sorted_api_list, (apiid,items[0]+"_id="+str(apiid)))
            bisect.insort(sorted_api_list, (apiid, items[0] + "_id=" + str(apiid)))
            id_counter += 1
            id_set.add(apiid)
            # print(id_counter, apiid, len(id_set))
            # print(id_set)
            assert (
                len(id_set) == id_counter
            ), "Duplicated api id generated or found when creating from existing"
            dst_validation_file.write(
                "PTI_CHECK_SUCCESS(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_"
                + api_group.upper()
                + ","
                + str(apiid)
                + ","
                + "&api_name));\n"
            )
            dst_validation_file.write(
                'EXPECT_EQ((std::strcmp(api_name, "' + items[0] + '") == 0), true);\n'
            )
        # f.write("    last_"+category+"_id=" + str(apiid) +",\n")
        # print(existing_apiids)
        for func in existing_apiids:
            apiid = existing_apiids[func]
            print("Existing: ", func, apiid)
            bisect.insort(sorted_api_list, (int(apiid), func + "=" + str(apiid)))
        for func in sorted_api_list:
            f.write("    " + func[1] + ",\n")
        f.write(" } pti_api_id_" + category + "_" + api_group + ";\n")
        f.write("\n")
        gen_apiid_terminate_header(f)


def generate_cb_api_file_info(cb_api_file, callbacks, category, api_group, regen=True):
    cb_api_file.write(
        "inline static std::map<uint32_t, const char*> pti_api_id_"
        + category
        + "_"
        + api_group
        + "_func_name =\n"
    )
    cb_api_file.write("  {\n")
    if regen:
        for func in callbacks:
            items = func.partition(",")
            cb_api_file.write(
                "    {pti_api_id_"
                + category
                + "_"
                + api_group
                + "::"
                + items[0]
                + '_id, "'
                + items[0]
                + '"},\n'
            )
    else:
        for key in callbacks.keys():
            items = key.partition("_")
            cb_api_file.write(
                "    {pti_api_id_"
                + category
                + "_"
                + api_group
                + "::"
                + key
                + ', "'
                + items[0]
                + '"},\n'
            )
    cb_api_file.write("  }; \n\n")


def generate_state_file_info(
    api_state_file, callbacks, category, api_group, regen=True
):
    api_state_file.write(
        "inline static std::map<uint32_t, uint32_t> pti_api_id_"
        + category
        + "_"
        + api_group
        + "_state =\n"
    )
    api_state_file.write("  {\n")
    if regen:
        for func in callbacks:
            items = func.partition(",")
            api_state_file.write(
                "    {pti_api_id_"
                + category
                + "_"
                + api_group
                + "::"
                + items[0]
                + "_id, "
                + "1"
                + "},\n"
            )
    else:
        for key in callbacks.keys():
            api_state_file.write(
                "    {pti_api_id_"
                + category
                + "_"
                + api_group
                + "::"
                + key
                + ", "
                + "1"
                + "},\n"
            )
    api_state_file.write("  }; \n\n")


#
# Generates Individual api_group related apiId files -- this is mapping of api functions to either generated apiId (where apiId is
#    missing from the api header file (e.g. ze_api.h) or parsed apiId (where apiId is found in api file).
# Input params:
#     dst_path -- where will these apiid files be generated.
#     cb_api_file -- file where the mapping from apiid -> function name will be kept -- this is needed for GetName from apiid.
#     category_dict -- internal dictionary of (category, api_group): function_list.  Function list is parsed from api headers.
#     existing_apiids_dict -- internal dictionary of parsed already known(existing) to pti apiid files in the dst_path.
#     api_state_file -- full path to file where we will put the std::maps for keeping state information of api_id enables/disables
#                       default is 1 -- that is all are enabled -- the state is controlled explicitly in the view_handler.
#     dst_validation_file -- full path to temporary file we use to generate a validation test case.
#     version_dict -- internal dictionary that keeps the parsed from api headers file version information (e.g from ze_api.h)
#     regen_api_files -- flag which notes to the function whether user is requesting the api id files to be regenerated.
#
def gen_apiid_files_per_category(
    dst_path,
    cb_api_file,
    category_dict,
    existing_apiids_dict,
    api_state_file,
    dst_validation_file,
    version_dict,
    regen_api_files,
):
    # for category, callback_list in category_dict.items():
    for cat_key, callback_list in category_dict.items():
        category = cat_key[0]
        api_group = cat_key[1]
        apiids_file_path = os.path.join(
            dst_path, "pti_" + category + "_" + api_group + "_api_ids.h"
        )
        existing_apiids = existing_apiids_dict.get(cat_key, {})
        print("Generation: Working with: ", api_group)
        print(existing_apiids.get("next_elem_index", 1))
        if regen_api_files:
            # pprint.pprint(existing_apiids)
            # pprint.pprint(callback_list)
            generate_api_file_portion(
                apiids_file_path,
                callback_list,
                category,
                api_group,
                existing_apiids,
                dst_validation_file,
                version_dict,
            )
            generate_cb_api_file_info(
                cb_api_file, callback_list, category, api_group, regen_api_files
            )

            generate_state_file_info(
                api_state_file, callback_list, category, api_group, regen_api_files
            )
        else:
            print(
                "Skipped api_id generation - rebuilding collateral files from existing api files"
            )
            del existing_apiids["next_elem_index"]
            generate_cb_api_file_info(
                cb_api_file, existing_apiids, category, api_group, regen_api_files
            )

            generate_state_file_info(
                api_state_file, existing_apiids, category, api_group, regen_api_files
            )


# Generates EnableTracing function and populates with registrable signatures for all apis: partitioned by all or just kfunc.
def gen_api(
    f,
    func_list,
    kfunc_list,
    exclude_from_epilogue_list,
    exclude_from_prologue_list,
    dst_api_dlsym_public_file,
    dst_api_dlsym_private_file,
    existing_apiid_dict,
    regen_api_files,
):
    f.write("void EnableTracer(zel_tracer_handle_t tracer) {\n")
    f.write("\n")
    f.write("  if (options_.api_tracing) {\n")
    cat_key = ("driver", "levelzero")
    existing_apiids = existing_apiid_dict.get(cat_key, {})
    for func in func_list:
        #        f.write(
        #            "    zelTracer"
        #            + func[2:]
        #            + "RegisterCallback("
        #            + "tracer, ZEL_REGISTER_PROLOGUE, "
        #            + func
        #            + "OnEnter);\n"
        #        )
        #        f.write(
        #            "    zelTracer"
        #            + func[2:]
        #            + "RegisterCallback("
        #            + "tracer, ZEL_REGISTER_EPILOGUE, "
        #            + func
        #            + "OnExit);\n"
        #        )
        dst_api_dlsym_private_file.write(
            "LEVEL_ZERO_LOADER_GET_SYMBOL(zelTracer" + func[2:] + "RegisterCallback);\n"
        )
        dst_api_dlsym_public_file.write(
            "decltype(&zelTracer"
            + func[2:]
            + "RegisterCallback) zelTracer"
            + func[2:]
            + "RegisterCallback_ = nullptr;  // NOLINT\n"
        )
        f.write(
            "    if (pti::PtiLzTracerLoader::Instance().zelTracer"
            + func[2:]
            + "RegisterCallback_) {\n"
        )
        f.write(
            "        pti::PtiLzTracerLoader::Instance().zelTracer"
            + func[2:]
            + "RegisterCallback_("
            + "tracer, ZEL_REGISTER_PROLOGUE, "
            + func
            + "OnEnter);\n"
        )
        f.write(
            "        pti::PtiLzTracerLoader::Instance().zelTracer"
            + func[2:]
            + "RegisterCallback_("
            + "tracer, ZEL_REGISTER_EPILOGUE, "
            + func
            + "OnExit);\n"
        )
        f.write("    }\n")
    f.write("  }\n")

    f.write("  else if (options_.kernel_tracing) {\n")
    for func in kfunc_list:
        # if func not in exclude_from_prologue_list:
        #    f.write(
        #        "    zelTracer"
        #        + func[2:]
        #        + "RegisterCallback("
        #        + "tracer, ZEL_REGISTER_PROLOGUE, "
        #        + func
        #        + "OnEnter);\n"
        #    )
        # if func not in exclude_from_epilogue_list:
        #    f.write(
        #        "    zelTracer"
        #        + func[2:]
        #        + "RegisterCallback("
        #        + "tracer, ZEL_REGISTER_EPILOGUE, "
        #        + func
        #        + "OnExit);\n"
        #    )
        f.write(
            "    if (pti::PtiLzTracerLoader::Instance().zelTracer"
            + func[2:]
            + "RegisterCallback_) {\n"
        )
        if func not in exclude_from_prologue_list:
            f.write(
                "        pti::PtiLzTracerLoader::Instance().zelTracer"
                + func[2:]
                + "RegisterCallback_("
                + "tracer, ZEL_REGISTER_PROLOGUE, "
                + func
                + "OnEnter);\n"
            )
        if func not in exclude_from_epilogue_list:
            f.write(
                "        pti::PtiLzTracerLoader::Instance().zelTracer"
                + func[2:]
                + "RegisterCallback_("
                + "tracer, ZEL_REGISTER_EPILOGUE, "
                + func
                + "OnExit);\n"
            )
        f.write("    }\n")
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
    with open(
        os.path.join(d, "levelzero", "ze_collector.h"), opener=default_file_opener
    ) as fp:
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
    existing_apiid_dict,
    regen_api_files,
    synchronization_viewkind_api_list,
):
    cat_key = ("driver", "levelzero")
    existing_apiids = existing_apiid_dict.get(cat_key, {})
    f.write("  [[maybe_unused]] ZeCollector* collector =\n")
    f.write("    static_cast<ZeCollector*>(global_data);\n")
    if func not in hybrid_mode_func_list:
        f.write("  if (collector->options_.hybrid_mode) return;\n")

    f.write("  [[maybe_unused]] uint64_t end_time_host = 0;\n")
    f.write("  end_time_host = utils::GetTime();\n")
    f.write("  ze_instance_data.end_time_host = end_time_host;\n")

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

    if func in synchronization_viewkind_api_list:
        f.write("  uint64_t synch_corrid = UniCorrId::GetUniCorrId();\n")

    if cb != "":
        f.write("  if (collector->options_.kernel_tracing) { \n")
        if (func in submission_func_list) or (func in synchronize_func_list_on_exit):
            if func in synchronization_viewkind_api_list:
                f.write(
                    "    "
                    + cb
                    + "(params, result, global_data, instance_user_data, &kids, synch_corrid); \n"
                )
            else:
                f.write(
                    "    "
                    + cb
                    + "(params, result, global_data, instance_user_data, &kids); \n"
                )
        else:
            if func in synchronization_viewkind_api_list:
                f.write(
                    "    "
                    + cb
                    + "(params, result, global_data, instance_user_data, synch_corrid); \n"
                )
            else:
                f.write(
                    "    "
                    + cb
                    + "(params, result, global_data, instance_user_data); \n"
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
    if regen_api_files or (func + "_id" in existing_apiids):
        # The following assert()'s are debug asserts and become noops in release builds
        f.write("  [[maybe_unused]] const char* api_name = nullptr;\n")
        f.write(
            "  assert(ptiViewGetApiIdName(pti_api_group_id::PTI_API_GROUP_LEVELZERO, "
            + func
            + "_id, &api_name) == pti_result::PTI_SUCCESS);\n"
        )
        f.write("  assert(std::strcmp(api_name, " + '"' + func + '") == 0);\n')
        f.write(
            "  uint32_t id_enabled=pti_api_id_driver_levelzero_state["
            + func
            + "_id];\n"
        )
        f.write(
            "  if (collector->cb_enabled_.fcallback && collector->fcallback_ != nullptr) {\n"
        )
        f.write(
            "    if ((collector->trace_all_env_value > 0) || ((collector->trace_all_env_value < 0) && id_enabled )) {\n"
        )
        f.write("       ZeKernelCommandExecutionRecord rec = {};\n")
        f.write("       rec.start_time_ = start_time_host;\n")
        f.write("       rec.end_time_ = end_time_host;\n")
        f.write("       rec.callback_id_ = " + func + "_id;\n")
        if func in km_rt_func_list:
            f.write("       if (sycl_data_kview.cid_) {\n")
            f.write("         rec.cid_ = sycl_data_kview.cid_;\n")
            f.write("       } else if (sycl_data_mview.cid_) {\n")
            f.write("         rec.cid_ = sycl_data_mview.cid_;\n")
            f.write("       } else {\n")
            f.write("         rec.cid_ = UniCorrId::GetUniCorrId();\n")
            f.write("       }\n")
        else:
            if func in synchronization_viewkind_api_list:
                f.write("       rec.cid_ = synch_corrid;\n")
            else:
                f.write("       rec.cid_ = UniCorrId::GetUniCorrId();\n")
        if func in km_rt_func_list:
            f.write("       sycl_data_kview.cid_ = 0;\n")
            f.write("       sycl_data_mview.cid_ = 0;\n")
        # f.write("    sycl_data_kview.cid_ = 0;\n")
        # f.write("    sycl_data_mview.cid_ = 0;\n")
        f.write("       rec.pid_ = thread_local_pid_tid_info.pid;\n")
        f.write("       rec.tid_ = thread_local_pid_tid_info.tid;\n")
        f.write("       rec.result_ = result;\n")
        f.write("       collector->fcallback_(collector->callback_data_, rec);\n")
        f.write("    }\n")
        f.write("  }\n")
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
    existing_apiid_dict,
    regen_api_files,
    synchronization_viewkind_api_list,
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
            existing_apiid_dict,
            regen_api_files,
            synchronization_viewkind_api_list,
        )
        f.write("}\n")
        f.write("\n")


def append_undefined_functions(func_list, func_dictionary):
    """
    In Level-Zero 14 that PTI uses to build - these functions are not defined yet.
    However, for Local profiling, PTI relies on these functions being present in the
    L0 that is installed on the system.
    We add these functions to the list of functions that are used to generate ApiId.
    ApiIds are needed for overhead measurments.
    We generate ApiIds for the tracing API functions as well. These functions also
    contribute to the overhead.
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
    if len(sys.argv) < 6:
        print(
            "Usage: python gen_tracing_header.py <output_include_path> <l0_include_path> <proj_bin_path> <pti_include_path> <ur_path> <regen_info> <regen_commit_hash>"
        )
        return

    print(
        "Using: python gen_tracing_header.py",
        sys.argv[1],
        sys.argv[2],
        sys.argv[3],
        sys.argv[4],
        sys.argv[5],
        sys.argv[6],
        sys.argv[7],
    )
    # TODO -- change the regen to take in a target L0 version to regenerate the api ids to.
    dst_path = sys.argv[1]
    if not os.path.exists(dst_path):
        os.mkdir(dst_path)

    dst_file_path = os.path.join(dst_path, "tracing.gen")
    if os.path.isfile(dst_file_path):
        os.remove(dst_file_path)
    dst_file = open(dst_file_path, "wt", opener=default_file_opener)

    dst_cb_api_file_path = os.path.join(dst_path, "tracing_cb_api.gen")
    if os.path.isfile(dst_cb_api_file_path):
        os.remove(dst_cb_api_file_path)
    dst_cb_api_file = open(dst_cb_api_file_path, "wt", opener=default_file_opener)

    dst_validation_file_with_path = os.path.join(
        dst_path, "tracing_base_apiid_validation_file.gen"
    )
    if os.path.isfile(dst_validation_file_with_path):
        os.remove(dst_validation_file_with_path)
    dst_validation_file = open(
        dst_validation_file_with_path, "wt", opener=default_file_opener
    )

    dst_api_dlsym_private_file_path = os.path.join(
        dst_path, "tracing_api_dlsym_private.gen"
    )
    dst_api_dlsym_public_file_path = os.path.join(
        dst_path, "tracing_api_dlsym_public.gen"
    )
    if os.path.isfile(dst_api_dlsym_private_file_path):
        os.remove(dst_api_dlsym_private_file_path)
        os.remove(dst_api_dlsym_public_file_path)
    dst_api_dlsym_private_file = open(
        dst_api_dlsym_private_file_path, "wt", opener=default_file_opener
    )
    dst_api_dlsym_public_file = open(
        dst_api_dlsym_public_file_path, "wt", opener=default_file_opener
    )

    l0_path = sys.argv[2]

    # Extract registratable apis from 2 headers (ze_api.h, layers/zel_tracing_register_cb.h)
    # Both are complementary to each other. One addresses apis from v1.0(ze_api.h) the other v1.1+.
    l0_file_path_api_1_0 = os.path.join(l0_path, "ze_api.h")
    l0_file_path_api_1_1_plus = os.path.join(
        l0_path, "layers", "zel_tracing_register_cb.h"
    )

    print("L0 files: ", l0_file_path_api_1_0, l0_file_path_api_1_1_plus)

    proj_bin_path = sys.argv[3]
    api_state_file_with_path = os.path.join(proj_bin_path, "pti_api_ids_state_maps.h")
    if os.path.isfile(api_state_file_with_path):
        os.remove(api_state_file_with_path)
    api_state_file = open(api_state_file_with_path, "wt", opener=default_file_opener)

    gen_apiid_header(api_state_file, "STATE_MAPS", ())
    api_state_file.write("#include <map>")
    api_state_file.write("\n\n")

    pti_inc_path = sys.argv[4]
    ur_path = sys.argv[5]
    ur_file_path = os.path.join(ur_path, "sycl/ur_api.h")

    print("Found UR Path: ", ur_path)
    regen_api_files = False  # Should we regenerate and update api_id_files as well?
    if sys.argv[6] == "ON":
        print("Regeneration of api files requested!")
        regen_api_files = True  # Should we regenerate and update api_id_files as well?

    l0_loader_info = sys.argv[7]

    # UNDO_WHEN_OCL ocl_api_list = get_ocl_api_list(ocl_path)
    ocl_api_list = []

    l0_api_files = [l0_file_path_api_1_0, l0_file_path_api_1_1_plus]
    (func_param_dictionary, l0_api_version) = gen_func_param_dict(l0_api_files)
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
        # "zeContextSystemBarrier",
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
        "zeFenceCreate",
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

    synchronization_viewkind_api_list = [
        "zeFenceHostSynchronize",
        "zeCommandListAppendBarrier",
        "zeCommandListAppendMemoryRangesBarrier",
        # "zeContextSystemBarrier",
        "zeEventHostSynchronize",
        "zeCommandQueueSynchronize",
        "zeCommandListHostSynchronize",
    ]

    exclude_from_epilogue_list = []

    exclude_from_prologue_list = ["zeCommandListHostSynchronize"]

    #######################################################
    # Callbacks API api_group/category lists with apis within
    # Max 64 categories can be supported
    #######################################################

    runtime_list = [
        "zeCommandListAppendLaunchKernel",
    ]

    resource_list = []

    (ur_func_list, ur_api_version) = get_ur_api_list(ur_file_path)
    # pprint.pprint(ur_func_list)

    # category_dict = {"runtime": ur_func_list, "driver": func_list}
    # category_dict = {"runtime": ur_func_list, "driver": func_list + ocl_api_list}
    # It is important to keep all categories(runtime,driver) together and maintain the order of api_groups(sycl, levelzero)
    #    in the category.
    category_dict = {
        ("runtime", "sycl"): ur_func_list,
        ("driver", "levelzero"): func_list,
        # ("driver", "opencl"): ocl_api_list,
    }

    version_dict = {
        "levelzero": (
            l0_api_version,
            l0_file_path_api_1_0.split("/")[-1],
            "https://github.com/oneapi-src/level-zero.git",
            l0_loader_info,
        ),
        "sycl": (
            ur_api_version,
            ur_file_path.split("/")[-1],
            "https://github.com/oneapi-src/unified-runtime.git",
            "",
        ),
    }

    existing_apiid_dict = get_apiids_per_category(pti_inc_path, category_dict)
    gen_callbacks(
        dst_file,
        func_param_dictionary,
        submission_func_list,
        synchronize_func_list_on_enter,
        synchronize_func_list_on_exit,
        hybrid_mode_func_list,
        km_rt_func_list,
        ze_gen_func_list,
        existing_apiid_dict,
        regen_api_files,
        synchronization_viewkind_api_list,
    )
    gen_api(
        dst_file,
        func_list,
        kfunc_list,
        exclude_from_epilogue_list,
        exclude_from_prologue_list,
        dst_api_dlsym_public_file,
        dst_api_dlsym_private_file,
        existing_apiid_dict,
        regen_api_files,
    )

    # Add functions that may be undefined in the L0 header used to build PTI,
    # but typically found in runtime (when using recent L0).
    # They will be used from the list of appended undefined functions here
    # to generate ApiIds for them. ApiIds are needed for overhead collection.
    append_undefined_functions(
        category_dict[("driver", "levelzero")], func_param_dictionary
    )

    gen_apiid_files_per_category(
        pti_inc_path,
        dst_cb_api_file,
        category_dict,
        existing_apiid_dict,
        api_state_file,
        dst_validation_file,
        version_dict,
        regen_api_files,
    )
    gen_apiid_terminate_header(api_state_file)

    dst_file.close()
    dst_cb_api_file.close()
    dst_validation_file.close()
    if not regen_api_files:
        os.remove(dst_validation_file_with_path)
    api_state_file.close()
    dst_api_dlsym_private_file.close()
    dst_api_dlsym_public_file.close()


if __name__ == "__main__":
    main()

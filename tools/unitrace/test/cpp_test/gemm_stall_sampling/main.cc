//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// GEMM test application covering the module/kernel layouts that matter for
// per-kernel analysis (kernel disassembly, EU stall sampling attribution).
//
// Level Zero is used directly rather than SYCL because the scenarios are about
// how kernels are grouped into modules and which of them are created and
// launched. SYCL hides all of that behind its own module management, so it
// cannot express "five kernels in the binary, three created, two launched".
//
// Scenarios (one per run, selected with --scenario):
//
//   1  one module, 1 kernel in the binary,  1 created, 1 launched
//   2  one module, 6 kernels in the binary, 6 created, 6 launched
//   3  three modules, 9 kernels total,      9 created, 9 launched
//   4  one module, 6 kernels in the binary, 3 created, 2 launched
//
// Scenario 4 is the interesting one for attribution: it keeps three populations
// deliberately different in size - kernels present in the module binary (6),
// kernels handed to zeKernelCreate (3), and kernels actually launched (2). That
// distinguishes a tool that disassembles module contents from one that only
// covers created kernels, and shows whether a created-but-never-launched kernel
// still gets an entry.
//
// One kernel of gemm_multi.spv, GEMM_Call, calls a device function defined in
// another source file, so its line-number program spans two files while its
// siblings' span one. Scenarios 2 and 3 launch it. That is the shape a real SYCL
// kernel has - a body interleaved with rows from the headers it calls into - and
// it is what catches a tool that hands every kernel of a module the same source
// lines: the mistake is invisible when each kernel's line table names one file.

#include <math.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "ze_utils.h"

#define ALIGN 64

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define BIAS_VALUE 0.5f
#define MAX_EPS 1.0e-4f

#define TILE_SIZE 16

// Module binaries. Scenario 1 needs a module whose binary holds exactly one
// kernel, which is why gemm_single.spv exists separately from gemm_multi.spv.
static const char* kSingleModule = "gemm_single.spv";
static const char* kMultiModule = "gemm_multi.spv";
static const char* kAuxModule = "gemm_aux.spv";

// How a kernel is launched and what it should produce. Kept as data so adding a
// kernel does not mean adding another branch to the launch path.
struct KernelSpec {
  const char* name;
  // Multiple of the plain GEMM result this kernel is expected to produce.
  float result_scale;
  // Adds BIAS_VALUE to every output element and takes the bias buffer as an
  // extra (4th) argument, shifting 'size' to index 4.
  bool has_bias;
  // Needs a fixed TILE_SIZE x TILE_SIZE work group and size % TILE_SIZE == 0,
  // rather than whatever zeKernelSuggestGroupSize returns.
  bool needs_tile_group;
};

static const KernelSpec kGemm = {"GEMM", 1.0f, false, false};
static const KernelSpec kGemmTiled = {"GEMM_Tiled", 1.0f, false, true};
static const KernelSpec kGemmScaled = {"GEMM_Scaled", 2.0f, false, false};
static const KernelSpec kGemmBias = {"GEMM_Bias", 1.0f, true, false};
static const KernelSpec kGemmAux = {"GEMM_Aux", 3.0f, false, false};
static const KernelSpec kGemmAuxScaled = {"GEMM_AuxScaled", 4.0f, false, false};
// Launched by scenarios 2 and 3; scenario 4 never creates it, leaving it present
// in the module binary but untouched by the run.
static const KernelSpec kGemmExtra = {"GEMM_Extra", 5.0f, false, false};
// The only kernel that calls a device function instead of being self-contained,
// which is what makes its line-number program span two source files. Its scale
// comes from GEMM_HELPER_SCALE in gemm_helper.h, so a wrong result here also means
// the call was not made.
static const KernelSpec kGemmCall = {"GEMM_Call", 7.0f, false, false};

// A kernel that has been created and is ready to launch.
struct LiveKernel {
  ze_kernel_handle_t handle;
  const KernelSpec* spec;
};

// A module that has been created, plus the kernels created from it.
struct LiveModule {
  ze_module_handle_t handle;
  std::string source;  // .spv file name, for diagnostics
  std::vector<LiveKernel> kernels;
};

static float Check(const std::vector<float>& a, float value) {
  // A zero expected value would make the relative error undefined; no scenario
  // produces one, but guard rather than silently returning inf.
  if (value == 0.0f) {
    return 1.0f;
  }
  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += fabs((a[i] - value) / value);
  }
  return eps / a.size();
}

// Load a .spv next to the executable and create a module from it. Returns
// nullptr (after printing why) if the file is missing or the driver rejects it.
static ze_module_handle_t CreateModule(ze_context_handle_t context, ze_device_handle_t device,
                                       const char* module_name) {
  std::vector<uint8_t> binary = utils::LoadBinaryFile(utils::GetExecutablePath() + module_name);
  if (binary.size() == 0) {
    std::cout << "[ERROR] Unable to find module " << module_name
              << ". The prebuilt .spv files are checked in beside this test's source and"
                 " copied next to the executable at build time." << std::endl;
    return nullptr;
  }

  ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                 nullptr,
                                 ZE_MODULE_FORMAT_IL_SPIRV,
                                 static_cast<uint32_t>(binary.size()),
                                 binary.data(),
                                 nullptr,
                                 nullptr};
  ze_module_handle_t module = nullptr;
  ze_module_build_log_handle_t build_log = nullptr;
  ze_result_t status = zeModuleCreate(context, device, &module_desc, &module, &build_log);
  if (status != ZE_RESULT_SUCCESS) {
    std::cout << "[ERROR] zeModuleCreate failed for " << module_name << " (status 0x" << std::hex
              << status << std::dec << ")" << std::endl;
    // The build log usually says exactly what the compiler disliked.
    if (build_log != nullptr) {
      size_t log_size = 0;
      if (zeModuleBuildLogGetString(build_log, &log_size, nullptr) == ZE_RESULT_SUCCESS &&
          log_size > 0) {
        std::vector<char> log(log_size);
        if (zeModuleBuildLogGetString(build_log, &log_size, log.data()) == ZE_RESULT_SUCCESS) {
          std::cout << "[ERROR] Build log: " << log.data() << std::endl;
        }
      }
    }
  }
  if (build_log != nullptr) {
    zeModuleBuildLogDestroy(build_log);
  }
  return module;
}

// Create one kernel from an already-created module.
static bool CreateKernel(ze_module_handle_t module, const KernelSpec* spec, LiveModule& out) {
  ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, spec->name};
  ze_kernel_handle_t kernel = nullptr;
  ze_result_t status = zeKernelCreate(module, &kernel_desc, &kernel);
  if (status != ZE_RESULT_SUCCESS || kernel == nullptr) {
    std::cout << "[ERROR] zeKernelCreate failed for " << spec->name << " (status 0x" << std::hex
              << status << std::dec << ")" << std::endl;
    return false;
  }
  out.kernels.push_back({kernel, spec});
  return true;
}

// Run one kernel once and return the mean relative error of its output.
// Returns a negative value if the launch could not be attempted.
static float RunAndCheck(const LiveKernel& lk, ze_device_handle_t device,
                         ze_context_handle_t context, const std::vector<float>& a,
                         const std::vector<float>& b, std::vector<float>& c,
                         const std::vector<float>& bias, unsigned size, float base_result) {
  const KernelSpec* spec = lk.spec;
  ze_kernel_handle_t kernel = lk.handle;
  ze_result_t status = ZE_RESULT_SUCCESS;

  uint32_t group_size[3] = {0, 0, 0};
  if (spec->needs_tile_group) {
    // The tiled kernel indexes fixed-size local arrays, so the group size is
    // dictated by the kernel, not suggested by the driver.
    group_size[0] = TILE_SIZE;
    group_size[1] = TILE_SIZE;
    group_size[2] = 1;
  } else {
    status = zeKernelSuggestGroupSize(kernel, size, size, 1, &(group_size[0]), &(group_size[1]),
                                      &(group_size[2]));
    if (status != ZE_RESULT_SUCCESS) {
      std::cout << "[ERROR] zeKernelSuggestGroupSize failed for " << spec->name << std::endl;
      return -1.0f;
    }
  }

  if (group_size[0] == 0 || group_size[1] == 0 ||
      (size % group_size[0]) != 0 || (size % group_size[1]) != 0) {
    std::cout << "Non-uniform workgroups are not supported (kernel " << spec->name << ")"
              << std::endl;
    return -1.0f;
  }

  const size_t matrix_bytes = static_cast<size_t>(size) * size * sizeof(float);

  ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr, 0, 0};
  void* dev_a = nullptr;
  void* dev_b = nullptr;
  void* dev_c = nullptr;
  void* dev_bias = nullptr;
  status = zeMemAllocDevice(context, &alloc_desc, matrix_bytes, ALIGN, device, &dev_a);
  if (status == ZE_RESULT_SUCCESS) {
    status = zeMemAllocDevice(context, &alloc_desc, matrix_bytes, ALIGN, device, &dev_b);
  }
  if (status == ZE_RESULT_SUCCESS) {
    status = zeMemAllocDevice(context, &alloc_desc, matrix_bytes, ALIGN, device, &dev_c);
  }
  if (status == ZE_RESULT_SUCCESS && spec->has_bias) {
    status = zeMemAllocDevice(context, &alloc_desc, size * sizeof(float), ALIGN, device, &dev_bias);
  }
  if (status != ZE_RESULT_SUCCESS) {
    std::cout << "[ERROR] Device allocation failed for " << spec->name << std::endl;
    // Free whatever did get allocated before bailing out.
    if (dev_a != nullptr) zeMemFree(context, dev_a);
    if (dev_b != nullptr) zeMemFree(context, dev_b);
    if (dev_c != nullptr) zeMemFree(context, dev_c);
    if (dev_bias != nullptr) zeMemFree(context, dev_bias);
    return -1.0f;
  }

  status = zeKernelSetGroupSize(kernel, group_size[0], group_size[1], group_size[2]);

  // Argument index 3 is either the bias buffer (then size moves to 4) or size.
  status = zeKernelSetArgumentValue(kernel, 0, sizeof(dev_a), &dev_a);
  status = zeKernelSetArgumentValue(kernel, 1, sizeof(dev_b), &dev_b);
  status = zeKernelSetArgumentValue(kernel, 2, sizeof(dev_c), &dev_c);
  if (spec->has_bias) {
    status = zeKernelSetArgumentValue(kernel, 3, sizeof(dev_bias), &dev_bias);
    status = zeKernelSetArgumentValue(kernel, 4, sizeof(size), &size);
  } else {
    status = zeKernelSetArgumentValue(kernel, 3, sizeof(size), &size);
  }

  ze_command_list_desc_t cmd_list_desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr, 0, 0};
  ze_command_list_handle_t cmd_list = nullptr;
  status = zeCommandListCreate(context, device, &cmd_list_desc, &cmd_list);

  status = zeCommandListAppendMemoryCopy(cmd_list, dev_a, a.data(), matrix_bytes, nullptr, 0,
                                         nullptr);
  status = zeCommandListAppendMemoryCopy(cmd_list, dev_b, b.data(), matrix_bytes, nullptr, 0,
                                         nullptr);
  if (spec->has_bias) {
    status = zeCommandListAppendMemoryCopy(cmd_list, dev_bias, bias.data(), size * sizeof(float),
                                           nullptr, 0, nullptr);
  }

  status = zeCommandListAppendBarrier(cmd_list, nullptr, 0, nullptr);

  ze_event_pool_desc_t event_pool_desc = {
      ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
      ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1};
  ze_event_pool_handle_t event_pool = nullptr;
  status = zeEventPoolCreate(context, &event_pool_desc, 0, nullptr, &event_pool);

  ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0, ZE_EVENT_SCOPE_FLAG_HOST,
                                ZE_EVENT_SCOPE_FLAG_HOST};
  ze_event_handle_t event = nullptr;
  zeEventCreate(event_pool, &event_desc, &event);

  ze_group_count_t dim = {size / group_size[0], size / group_size[1], 1};
  status = zeCommandListAppendLaunchKernel(cmd_list, kernel, &dim, event, 0, nullptr);

  status = zeCommandListAppendBarrier(cmd_list, nullptr, 0, nullptr);

  status = zeCommandListAppendMemoryCopy(cmd_list, c.data(), dev_c, matrix_bytes, nullptr, 0,
                                         nullptr);

  status = zeCommandListClose(cmd_list);

  ze_command_queue_desc_t cmd_queue_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,     nullptr, 0, 0, 0,
      ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,       ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
  ze_command_queue_handle_t cmd_queue = nullptr;
  status = zeCommandQueueCreate(context, device, &cmd_queue_desc, &cmd_queue);

  status = zeCommandQueueExecuteCommandLists(cmd_queue, 1, &cmd_list, nullptr);
  status = zeCommandQueueSynchronize(cmd_queue, UINT32_MAX);

  status = zeCommandQueueDestroy(cmd_queue);
  status = zeCommandListDestroy(cmd_list);

  status = zeMemFree(context, dev_a);
  status = zeMemFree(context, dev_b);
  status = zeMemFree(context, dev_c);
  if (dev_bias != nullptr) {
    status = zeMemFree(context, dev_bias);
  }

  ze_device_properties_t props{};
  props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES_1_2;
  status = zeDeviceGetProperties(device, &props);

  ze_kernel_timestamp_result_t timestamp{};
  status = zeEventQueryKernelTimestamp(event, &timestamp);
  status = zeEventDestroy(event);
  status = zeEventPoolDestroy(event_pool);

  double time = 0.0;
  if (props.timerResolution != 0) {
    time = static_cast<double>(timestamp.global.kernelEnd - timestamp.global.kernelStart) /
           props.timerResolution;
  }
  std::cout << "  " << spec->name << ": " << time << " ns" << std::endl;

  // Each kernel scales the plain GEMM result differently, so a wrong-kernel
  // launch shows up as an accuracy failure rather than passing unnoticed.
  float expected = base_result * spec->result_scale;
  if (spec->has_bias) {
    expected += BIAS_VALUE;
  }
  return Check(c, expected);
}

// Which modules a scenario builds, and which kernels it creates from each.
struct ScenarioPlan {
  const char* description;
  // Per module: the .spv to load and the kernels to create from it.
  struct ModulePlan {
    const char* module;
    std::vector<const KernelSpec*> create;
    // Kernels created but intentionally not launched.
    std::vector<const KernelSpec*> skip_launch;
  };
  std::vector<ModulePlan> modules;
};

static ScenarioPlan GetScenarioPlan(int scenario) {
  switch (scenario) {
    case 1:
      // Simplest case: a module whose binary contains exactly one kernel.
      return {"1 module, 1 kernel in binary, 1 created, 1 launched",
              {{kSingleModule, {&kGemm}, {}}}};
    case 2:
      // All six kernels of one module created and launched, GEMM_Call among them.
      return {"1 module, 6 kernels in binary, 6 created, 6 launched",
              {{kMultiModule,
                {&kGemm, &kGemmTiled, &kGemmScaled, &kGemmBias, &kGemmExtra, &kGemmCall},
                {}}}};
    case 3:
      // Three modules live at once, spanning both a single-kernel and two
      // multi-kernel binaries. Every kernel is created and launched.
      return {"3 modules, 9 kernels in binaries, 9 created, 9 launched",
              {{kSingleModule, {&kGemm}, {}},
               {kMultiModule,
                {&kGemm, &kGemmTiled, &kGemmScaled, &kGemmBias, &kGemmExtra, &kGemmCall},
                {}},
               {kAuxModule, {&kGemmAux, &kGemmAuxScaled}, {}}}};
    case 4:
      // Three populations, all deliberately different in size: 6 kernels in the
      // binary, 3 created, 2 launched. GEMM_Bias, GEMM_Extra and GEMM_Call are
      // never created; GEMM_Tiled is created but never launched.
      return {"1 module, 6 kernels in binary, 3 created, 2 launched",
              {{kMultiModule, {&kGemm, &kGemmScaled, &kGemmTiled}, {&kGemmTiled}}}};
    default:
      return {nullptr, {}};
  }
}

static void PrintScenarios() {
  std::cout << "Scenarios:" << std::endl;
  for (int i = 1; i <= 4; ++i) {
    ScenarioPlan plan = GetScenarioPlan(i);
    std::cout << "  " << i << "  " << plan.description << std::endl;
  }
}

static bool IsLaunchSkipped(const ScenarioPlan::ModulePlan& mp, const KernelSpec* spec) {
  for (size_t i = 0; i < mp.skip_launch.size(); ++i) {
    if (mp.skip_launch[i] == spec) {
      return true;
    }
  }
  return false;
}

// Returns true if every launched kernel produced the expected result.
static bool RunScenario(int scenario, ze_device_handle_t device, ze_driver_handle_t driver,
                        unsigned size, unsigned iterations) {
  ScenarioPlan plan = GetScenarioPlan(scenario);
  if (plan.description == nullptr) {
    std::cout << "[ERROR] Unknown scenario " << scenario << std::endl;
    return false;
  }

  std::cout << "Scenario " << scenario << ": " << plan.description << std::endl;

  ze_context_handle_t context = utils::ze::GetContext(driver);
  if (context == nullptr) {
    std::cout << "[ERROR] Unable to create context" << std::endl;
    return false;
  }

  std::vector<float> a(static_cast<size_t>(size) * size, A_VALUE);
  std::vector<float> b(static_cast<size_t>(size) * size, B_VALUE);
  std::vector<float> c(static_cast<size_t>(size) * size, 0.0f);
  std::vector<float> bias(size, BIAS_VALUE);
  const float base_result = A_VALUE * B_VALUE * size;

  // Create every module and kernel up front, so all modules in a
  // multiple-module scenario are alive at the same time. Creating and
  // destroying them one at a time would not exercise that.
  std::vector<LiveModule> live_modules;
  bool setup_ok = true;
  for (size_t m = 0; m < plan.modules.size() && setup_ok; ++m) {
    const ScenarioPlan::ModulePlan& mp = plan.modules[m];
    ze_module_handle_t module = CreateModule(context, device, mp.module);
    if (module == nullptr) {
      setup_ok = false;
      break;
    }

    LiveModule lm;
    lm.handle = module;
    lm.source = mp.module;
    for (size_t k = 0; k < mp.create.size(); ++k) {
      if (!CreateKernel(module, mp.create[k], lm)) {
        setup_ok = false;
        break;
      }
    }
    live_modules.push_back(lm);
  }

  bool all_correct = setup_ok;
  if (setup_ok) {
    for (unsigned it = 0; it < iterations; ++it) {
      std::cout << "Iteration " << (it + 1) << " of " << iterations << std::endl;
      for (size_t m = 0; m < live_modules.size(); ++m) {
        const ScenarioPlan::ModulePlan& mp = plan.modules[m];
        LiveModule& lm = live_modules[m];
        for (size_t k = 0; k < lm.kernels.size(); ++k) {
          const LiveKernel& lk = lm.kernels[k];
          if (IsLaunchSkipped(mp, lk.spec)) {
            // Created on purpose but never launched.
            continue;
          }
          // The tiled kernel needs the matrix to divide into whole tiles.
          if (lk.spec->needs_tile_group && (size % TILE_SIZE) != 0) {
            std::cout << "  " << lk.spec->name << ": skipped (size " << size
                      << " is not a multiple of " << TILE_SIZE << ")" << std::endl;
            continue;
          }
          float eps = RunAndCheck(lk, device, context, a, b, c, bias, size, base_result);
          if (eps < 0.0f) {
            all_correct = false;
            continue;
          }
          if (eps >= MAX_EPS) {
            std::cout << "[ERROR] " << lk.spec->name << " in " << lm.source
                      << " produced INCORRECT results, accuracy: " << eps << std::endl;
            all_correct = false;
          }
        }
      }
    }
  }

  // Tear down in reverse: kernels before their module.
  for (size_t m = 0; m < live_modules.size(); ++m) {
    for (size_t k = 0; k < live_modules[m].kernels.size(); ++k) {
      zeKernelDestroy(live_modules[m].kernels[k].handle);
    }
    zeModuleDestroy(live_modules[m].handle);
  }
  zeContextDestroy(context);

  return all_correct;
}

static void PrintUsage() {
  std::cout << "Usage: gemm_stall_sampling [options]" << std::endl
            << "  --scenario N     scenario to run, 1-4 (default 1)" << std::endl
            << "  --size N         matrix dimension, N x N (default 256)" << std::endl
            << "  --iterations N   repeats of the whole scenario (default 4)" << std::endl
            << "  --list           list the scenarios and exit" << std::endl
            << "  --help           show this message and exit" << std::endl;
}

// Parse a positive integer option value. Returns false (after printing) if the
// value is missing or not a usable number, so a typo fails loudly instead of
// silently running the default.
static bool ParseUnsigned(int argc, char* argv[], int& i, const char* option, unsigned& out) {
  if (i + 1 >= argc) {
    std::cout << "[ERROR] " << option << " requires a value" << std::endl;
    return false;
  }
  ++i;
  try {
    unsigned long value = std::stoul(argv[i]);
    if (value == 0) {
      std::cout << "[ERROR] " << option << " must be greater than 0" << std::endl;
      return false;
    }
    out = static_cast<unsigned>(value);
  } catch (const std::exception&) {
    std::cout << "[ERROR] " << option << " expects a number, got '" << argv[i] << "'" << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char* argv[]) {
  unsigned scenario = 1;
  unsigned size = 256;
  unsigned iterations = 4;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--scenario") == 0) {
      if (!ParseUnsigned(argc, argv, i, "--scenario", scenario)) {
        return 1;
      }
    } else if (strcmp(argv[i], "--size") == 0) {
      if (!ParseUnsigned(argc, argv, i, "--size", size)) {
        return 1;
      }
    } else if (strcmp(argv[i], "--iterations") == 0) {
      if (!ParseUnsigned(argc, argv, i, "--iterations", iterations)) {
        return 1;
      }
    } else if (strcmp(argv[i], "--list") == 0) {
      PrintScenarios();
      return 0;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return 0;
    } else {
      std::cout << "[ERROR] Unknown option '" << argv[i] << "'" << std::endl;
      PrintUsage();
      return 1;
    }
  }

  if (scenario < 1 || scenario > 4) {
    std::cout << "[ERROR] --scenario must be 1-4, got " << scenario << std::endl;
    PrintScenarios();
    return 1;
  }

  ze_result_t status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  if (status != ZE_RESULT_SUCCESS) {
    std::cout << "[ERROR] Failed to initialize Level Zero" << std::endl;
    return 1;
  }

  ze_device_handle_t device = utils::ze::GetGpuDevice();
  ze_driver_handle_t driver = utils::ze::GetGpuDriver();
  if (device == nullptr || driver == nullptr) {
    std::cout << "Unable to find GPU device" << std::endl;
    return 0;
  }

  std::cout << "Level Zero GEMM scenarios (matrix " << size << " x " << size << ", " << iterations
            << " iterations)" << std::endl;
  std::cout << "Target device: " << utils::ze::GetDeviceName(device) << std::endl;

  auto start = std::chrono::steady_clock::now();
  bool ok = RunScenario(static_cast<int>(scenario), device, driver, size, iterations);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;

  std::cout << "Total execution time: " << time.count() << " s" << std::endl;
  if (!ok) {
    std::cout << "[ERROR] Scenario " << scenario << " failed" << std::endl;
    return 1;
  }
  std::cout << "Scenario " << scenario << " completed successfully" << std::endl;
  return 0;
}

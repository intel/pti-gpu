//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
//
// Fuzz harness for the generic CLI argument parser (cli_options.h). It parses
// with its own option table, not unitrace's kCliOptions: the values unitrace's
// handlers and FinalizeParse() convert (device and MPI rank lists, device index,
// Chrome event buffer size) are fuzzed by fuzz_sample_lists and fuzz_env_numbers.
//
// Defines a minimal option table that exercises every CliOption variant
// (Bool, BoolH, Value with validator, ValueH, Action) and feeds fuzz-derived
// argv arrays through cli::ParseArgs(). The Value options have no environment
// variable: glibc keeps every string set with setenv(), so fuzz-derived values
// there would grow memory over a campaign. --flag-a still sets one, to "1".

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <sstream>

#include "cli_options.h"
#include "json_check.h"

// Minimal context struct exercising all handler paths.
struct FuzzCtx {
  bool flag_a = false;
  bool flag_b = false;
  std::string value_c;
  std::string value_d;
  bool action_fired = false;
};

// Validator: accepts only alphanumeric strings.
static bool ValidateAlnum(const std::string &s) {
  for (char c : s) {
    if (!std::isalnum(static_cast<unsigned char>(c))) return false;
  }
  return !s.empty();
}

// Validator: accepts only numeric strings.
static bool ValidateNumeric(const std::string &s) {
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return !s.empty();
}

// Handler for BoolH: sets a flag on the context.
static cli::Status HandleBoolH(FuzzCtx &ctx,
                                const cli::CliOption<FuzzCtx> &,
                                const char *) {
  ctx.flag_b = !ctx.flag_b;
  return cli::Status::Ok;
}

// Handler for ValueH: stores a string on the context.
static cli::Status HandleValueH(FuzzCtx &ctx,
                                 const cli::CliOption<FuzzCtx> &,
                                 const char *value) {
  ctx.value_d = value ? value : "";
  return cli::Status::Ok;
}

// Handler for Action: simulates --help / --version.
static cli::Status HandleAction(FuzzCtx &ctx,
                                 const cli::CliOption<FuzzCtx> &,
                                 const char *) {
  ctx.action_fired = true;
  return cli::Status::ExitOk;
}

using Opt = cli::Factory<FuzzCtx>;

static constexpr cli::CliOption<FuzzCtx> kFuzzOptions[] = {
    Opt::Bool("--flag-a", "-a", "core", "A boolean flag", "FUZZ_FLAG_A"),
    Opt::BoolH("--flag-b", "-b", "core", "A handled boolean flag", HandleBoolH),
    Opt::Value("--value-c", "-c", "string", "core", "name",
               "A validated string value", nullptr, ValidateAlnum),
    Opt::Value("--value-num", "-n", "int", "core", "number",
               "A numeric value", nullptr, ValidateNumeric, "0"),
    Opt::ValueH("--value-d", "-d", "string", "core", "data",
                "A handled value", HandleValueH),
    Opt::Action("--action", "core", "A print-and-exit action", HandleAction),
};

static constexpr size_t kNumOptions =
    sizeof(kFuzzOptions) / sizeof(kFuzzOptions[0]);

// libFuzzer entry point: one input is one command line.
//   data, size  the argv strings, separated by NUL bytes (at most 64 are used);
//               argv[0] is "fuzz_unitrace"
// Parses them with cli::ParseArgs() against kFuzzOptions, then checks that
// cli::JsonEscape() of every argument is a valid JSON string. Returns 0; a crash,
// a sanitizer report or the trap is the finding.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Build argv by splitting fuzz input on null bytes.
  std::vector<std::string> tokens;
  tokens.push_back("fuzz_unitrace");

  const char *begin = reinterpret_cast<const char *>(data);
  const char *end = begin + size;
  const char *p = begin;

  while (p < end) {
    const char *nul = static_cast<const char *>(
        std::memchr(p, '\0', static_cast<size_t>(end - p)));
    if (nul == nullptr) {
      tokens.emplace_back(p, end);
      break;
    }
    tokens.emplace_back(p, nul);
    p = nul + 1;
  }

  // Limit to avoid combinatorial explosion.
  if (tokens.size() > 64) {
    tokens.resize(64);
  }

  // Build the char* argv.
  std::vector<char *> argv;
  argv.reserve(tokens.size());
  for (auto &t : tokens) {
    argv.push_back(t.data());
  }

  FuzzCtx ctx;
  int argc = static_cast<int>(argv.size());
  (void)cli::ParseArgs<FuzzCtx>(argc, argv.data(), kFuzzOptions, kNumOptions,
                                 ctx);

  // Oracle: JsonEscape (used for --print-options-schema) must produce a valid JSON string.
  for (const auto &t : tokens) {
    std::ostringstream os;
    cli::JsonEscape(os, t.c_str());
    if (!json_check::IsValid(os.str())) __builtin_trap();
  }

  return 0;
}

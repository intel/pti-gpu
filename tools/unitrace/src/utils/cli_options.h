//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_UTILS_CLI_OPTIONS_H_
#define PTI_TOOLS_UNITRACE_UTILS_CLI_OPTIONS_H_

// Generic table-driven CLI parser: one constexpr CliOption<Ctx>[] table drives
// argument dispatch, --help text, and a JSON schema dump. `Ctx` is the
// caller's own business-state struct, threaded through to Handler.

#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>

#include "utils.h"

namespace cli {

// Handler return status: continue parsing, exit with a command-line error
// (ParseArgs returns -1), or exit cleanly (ParseArgs returns 0, e.g. --help).
enum class Status { Ok, Error, ExitOk };

using Validator = bool (*)(const std::string&);

template <typename Ctx>
struct CliOption;

template <typename Ctx>
using Handler = Status (*)(Ctx&, const CliOption<Ctx>&, const char* value);

template <typename Ctx>
struct CliOption {
  const char* name_;        // "--call-logging"
  const char* alias_;       // "-c" or nullptr
  const char* type_;        // "bool"|"int"|"string"|"enum"|"path"|"action"
  const char* group_;       // caller-defined group id, e.g. "core"|"output"
  const char* value_hint_;  // "number-of-events" -> "<number-of-events>"; nullptr for bool/action
  const char* def_;         // default as a string ("false","-1","50","ComputeBasic"); nullptr
  const char* choices_;     // comma-separated ("0,1") or nullptr
  bool        hidden_;      // excluded from Usage; still emitted to JSON with hidden:true
  const char* help_;        // the single help string

  // ---- behavior (what the parser executes) ----
  const char* env_;         // "UNITRACE_CallLogging"; nullptr when a handler does its own SetEnv
  Validator   validate_;    // nullptr = no validation
  Handler<Ctx> handler_;    // nullptr for plain data rows; set for multi-env/stateful/exit rows

  // ---- schema-only relations (emitted to the JSON schema for a launcher UI;
  //      the parser itself does not read these). Set via the chained modifiers
  //      below so only the few flags that have a relation carry one. ----
  const char* path_kind_      = nullptr; // "file" | "directory" for a path flag
  const char* conflicts_with_ = nullptr; // comma-separated flags that cannot be combined with this
  const char* requires_any_   = nullptr; // comma-separated flags, at least one of which must be active
  const char* choices_from_   = nullptr; // dynamic choice source resolved by the client, e.g. "metric-groups"
  bool        schema_omit_    = false;   // stays in --help and dispatch, but excluded from the JSON schema

  constexpr CliOption PathFile() const { CliOption o = *this; o.path_kind_ = "file";      return o; }
  constexpr CliOption PathDir()  const { CliOption o = *this; o.path_kind_ = "directory"; return o; }
  constexpr CliOption Conflicts(const char* c) const { CliOption o = *this; o.conflicts_with_ = c; return o; }
  constexpr CliOption Requires(const char* r)  const { CliOption o = *this; o.requires_any_   = r; return o; }
  constexpr CliOption ChoicesFrom(const char* c) const { CliOption o = *this; o.choices_from_ = c; return o; }
  constexpr CliOption SchemaOmit() const { CliOption o = *this; o.schema_omit_ = true; return o; }
};

// Factories for the CliOption<Ctx> rows of an option table. Bind Ctx once with
// `using Opt = cli::Factory<MyCtx>;`, then build rows as Opt::Bool(...), Opt::Value(...).
template <typename Ctx>
struct Factory {
  using Opt = CliOption<Ctx>;
  using HandlerFn = Handler<Ctx>;

  // Boolean switch (no value) that sets `env` to "1".
  static constexpr Opt Bool(const char* name, const char* alias, const char* group,
                            const char* help, const char* env, bool hidden = false) {
    return {name, alias, "bool", group, nullptr, "false", nullptr, hidden, help, env, nullptr, nullptr};
  }

  // Boolean switch whose behavior is a handler (multi-env / stateful), not an env var.
  static constexpr Opt BoolH(const char* name, const char* alias, const char* group,
                             const char* help, HandlerFn handler, bool hidden = false) {
    return {name, alias, "bool", group, nullptr, "false", nullptr, hidden, help, nullptr, nullptr, handler};
  }

  // Value flag (takes the next argv token) that sets `env` to that value.
  static constexpr Opt Value(const char* name, const char* alias, const char* type,
                             const char* group, const char* value_hint, const char* help,
                             const char* env, Validator validate = nullptr,
                             const char* def = nullptr, const char* choices = nullptr,
                             bool hidden = false) {
    return {name, alias, type, group, value_hint, def, choices, hidden, help, env, validate, nullptr};
  }

  // Value flag whose behavior is a handler (multi-env / stateful), not an env var.
  static constexpr Opt ValueH(const char* name, const char* alias, const char* type,
                              const char* group, const char* value_hint, const char* help,
                              HandlerFn handler) {
    return {name, alias, type, group, value_hint, nullptr, nullptr, false, help, nullptr, nullptr, handler};
  }

  // Print-and-exit flag (no value): the handler prints and returns Status::ExitOk.
  static constexpr Opt Action(const char* name, const char* group, const char* help,
                              HandlerFn handler, bool hidden = false) {
    return {name, nullptr, "action", group, nullptr, nullptr, nullptr, hidden, help, nullptr, nullptr, handler};
  }
};

// Group id -> human label, for a launcher form's section headers.
struct CliGroup { const char* id_; const char* label_; };

template <typename Ctx>
inline bool IsValueType(const CliOption<Ctx>& o) {
  return strcmp(o.type_, "bool") != 0 && strcmp(o.type_, "action") != 0;
}

// Match an argv token against a flag name or its short alias.
template <typename Ctx>
inline const CliOption<Ctx>* Lookup(const CliOption<Ctx>* table, size_t n, const char* arg) {
  for (size_t i = 0; i < n; ++i) {
    const CliOption<Ctx>& o = table[i];
    if (strcmp(arg, o.name_) == 0 || (o.alias_ && strcmp(arg, o.alias_) == 0)) {
      return &o;
    }
  }
  return nullptr;
}

// Generic dispatch loop. Returns >0 (app_index, where the app begins), 0
// (handled and exited cleanly, e.g. --help), or -1 (command-line error).
// Callers with post-loop logic (defaults, conflict checks) run it themselves
// after a positive app_index -- that's caller-specific, not engine logic.
template <typename Ctx>
int ParseArgs(int argc, char* argv[], const CliOption<Ctx>* table, size_t n, Ctx& ctx) {
  int app_index = 1;
  for (int i = 1; i < argc; ++i) {
    const CliOption<Ctx>* o = Lookup<Ctx>(table, n, argv[i]);
    if (!o) {
      break;  // first token that is not a known flag => the application begins here
    }

    const char* value = nullptr;
    if (IsValueType(*o)) {
      if (++i >= argc) {
        std::cerr << "[ERROR] Option " << o->name_ << " requires a value" << std::endl;
        return -1;
      }
      value = argv[i];
      if (o->validate_ && !o->validate_(value)) {
        std::cerr << "[ERROR] Invalid value for " << o->name_ << ": " << value << std::endl;
        return -1;
      }
    }

    if (o->handler_) {
      switch (o->handler_(ctx, *o, value)) {
        case Status::Error:  return -1;
        case Status::ExitOk: return 0;
        case Status::Ok:     break;
      }
    } else if (o->env_) {
      utils::SetEnv(o->env_, value ? value : "1");
    }

    app_index += IsValueType(*o) ? 2 : 1;
  }

  return app_index;
}

inline std::string PadRight(std::string s, size_t width) {
  s.append((s.size() < width) ? (width - s.size()) : 1, ' ');
  return s;
}

// Print --help text: name [alias] <value-hint>, padded, then the help string.
// Skips hidden rows.
template <typename Ctx>
void Usage(std::ostream& os, const char* progname, const CliOption<Ctx>* table, size_t n) {
  os << "Usage: " << progname << " [options] <application> <args>" << std::endl;
  os << "Options:" << std::endl;
  for (size_t i = 0; i < n; ++i) {
    const CliOption<Ctx>& o = table[i];
    if (o.hidden_) {
      continue;
    }
    std::string left = o.name_;
    if (o.alias_) {
      left += std::string(" [") + o.alias_ + "]";
    }
    if (o.value_hint_) {
      left += std::string(" <") + o.value_hint_ + ">";
    }
    os << PadRight(left, 34) << o.help_ << std::endl;
  }
}

inline void JsonEscape(std::ostream& os, const char* s) {
  os << '"';
  for (const char* p = s; p && *p; ++p) {
    unsigned char c = static_cast<unsigned char>(*p);
    switch (c) {
      case '"':  os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\b': os << "\\b";  break;
      case '\f': os << "\\f";  break;
      case '\n': os << "\\n";  break;
      case '\r': os << "\\r";  break;
      case '\t': os << "\\t";  break;
      default:
        if (c < 0x20) {  // other control chars must be \u-escaped for valid JSON
          static const char* hex = "0123456789abcdef";
          os << "\\u00" << hex[(c >> 4) & 0xf] << hex[c & 0xf];
        } else {
          os << *p;
        }
    }
  }
  os << '"';
}

// Emit a comma-separated list (e.g. "0,1" or "--a,--b") as a JSON string array.
inline void JsonStringArray(std::ostream& os, const char* csv) {
  os << "[";
  std::string s = csv;
  size_t start = 0;
  bool first = true;
  while (start <= s.size()) {
    size_t comma = s.find(',', start);
    std::string one = (comma == std::string::npos)
        ? s.substr(start) : s.substr(start, comma - start);
    if (!first) {
      os << ", ";
    }
    first = false;
    JsonEscape(os, one.c_str());
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
  os << "]";
}

// Emit the option schema as JSON: flag attributes, group labels, and the
// schema-only relations set via CliOption's chained modifiers. SchemaOmit()
// flags are excluded. `header` lets the caller prepend its own top-level
// fields (schemaVersion, tool version, ...) before groups/flags.
template <typename Ctx>
void PrintOptionsSchema(std::ostream& os, const CliGroup* groups, size_t n_groups,
                        const CliOption<Ctx>* table, size_t n_flags,
                        const std::string& header) {
  os << "{\n";
  os << header;
  os << "  \"groups\": [\n";
  bool first_group = true;
  for (size_t i = 0; i < n_groups; ++i) {
    const CliGroup& g = groups[i];
    if (!first_group) {
      os << ",\n";
    }
    first_group = false;
    os << "    {\"id\": ";  JsonEscape(os, g.id_);
    os << ", \"label\": ";  JsonEscape(os, g.label_);
    os << "}";
  }
  os << "\n  ],\n";
  os << "  \"flags\": [\n";
  bool first = true;
  for (size_t i = 0; i < n_flags; ++i) {
    const CliOption<Ctx>& o = table[i];
    if (o.schema_omit_) {
      continue;  // in --help and dispatch, but not the machine schema
    }
    if (!first) {
      os << ",\n";
    }
    first = false;
    os << "    {";
    os << "\"name\": ";  JsonEscape(os, o.name_);
    if (o.alias_) {
      os << ", \"alias\": ";  JsonEscape(os, o.alias_);
    }
    os << ", \"type\": ";   JsonEscape(os, o.type_);
    os << ", \"group\": ";  JsonEscape(os, o.group_);
    if (o.value_hint_) {
      os << ", \"valueHint\": ";  JsonEscape(os, o.value_hint_);
    }
    if (o.def_) {
      os << ", \"default\": ";  JsonEscape(os, o.def_);
    }
    if (o.choices_) {
      os << ", \"choices\": ";  JsonStringArray(os, o.choices_);
    }
    if (o.path_kind_) {
      os << ", \"pathKind\": ";  JsonEscape(os, o.path_kind_);
    }
    if (o.conflicts_with_) {
      os << ", \"conflictsWith\": ";  JsonStringArray(os, o.conflicts_with_);
    }
    if (o.requires_any_) {
      os << ", \"requires\": ";  JsonStringArray(os, o.requires_any_);
    }
    if (o.choices_from_) {
      os << ", \"choicesFrom\": ";  JsonEscape(os, o.choices_from_);
    }
    if (o.hidden_) {
      os << ", \"hidden\": true";
    }
    if (o.help_) {
      os << ", \"help\": ";  JsonEscape(os, o.help_);
    }
    os << "}";
  }
  os << "\n  ]\n}\n";
}

}  // namespace cli

#endif  // PTI_TOOLS_UNITRACE_UTILS_CLI_OPTIONS_H_

#ifndef PTI_UTILS_DEMANGLE_H_
#define PTI_UTILS_DEMANGLE_H_

#if __has_include(<cxxabi.h>)
#define HAVE_CXXABI 1
#include <cxxabi.h>
#include <cstring>
#else
#define HAVE_CXXABI 0
#endif
#include <string>

#include "pti_assert.h"

namespace utils {

static inline std::string Demangle(const char* name) {
  PTI_ASSERT(name != nullptr);

#if HAVE_CXXABI
  int status = 0;
  char* demangled = abi::__cxa_demangle(name, nullptr, 0, &status);
  if (status != 0) {
    return name;
  }

  constexpr const char* const prefix_to_skip = "typeinfo name for ";
  const size_t prefix_to_skip_len = strlen(prefix_to_skip);
  const size_t shift =
    (std::strncmp(demangled, prefix_to_skip, prefix_to_skip_len) == 0) ?
    prefix_to_skip_len : 0;

  std::string result(demangled + shift);
  free(demangled);
  return result;
#else
  return name;
#endif
}

} // namespace utils

#undef HAVE_CXXABI

#endif // PTI_UTILS_DEMANGLE_H_
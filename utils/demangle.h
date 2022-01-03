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

namespace utils {

static inline std::string demangle(const char* name) {
  if (!name)
  {
    return std::string{};
  }
#if HAVE_CXXABI
  int status;
  char *demangled = abi::__cxa_demangle(name, nullptr, 0, &status);
  if (status != 0) {
    return name;
  }
  
  constexpr const char *const prefixToSkip = "typeinfo name for ";
  const size_t prefixToSkipLen = strlen(prefixToSkip);
  const size_t shift = (std::strncmp(demangled, prefixToSkip, prefixToSkipLen) == 0) ? prefixToSkipLen : 0;

  std::string retVal(demangled + shift);
  free(demangled);
  return retVal;
#else
  return name;
#endif
}

} // namespace utils

#undef HAVE_CXXABI

#endif // PTI_UTILS_DEMANGLE_H_

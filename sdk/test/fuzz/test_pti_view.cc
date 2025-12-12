#include <pti/pti_view.h>
#include <stddef.h>

#include <cstddef>
#include <cstring>
#include <type_traits>

///////////////////////////////////////////////////////////////////////////////
/// @ Filters out function from undefined behaviour sanitizer while fuzzing
#if defined(__clang__) || defined(__GNUC__)
#define ATTRIBUTE_NO_SANITIZE_UNDEFINED __attribute__((no_sanitize("undefined")))
#else
#define ATTRIBUTE_NO_SANITIZE_UNDEFINED
#endif

namespace {
template <typename T>
ATTRIBUTE_NO_SANITIZE_UNDEFINED inline T ConvertByteArray(const unsigned char* byte_array) {
  static_assert(std::is_trivially_copyable<T>::value,
                "Must convert from byte array to trivially copyable type or "
                "risk undefined behavior.");

  T converted_value;
  std::memcpy(&converted_value, &byte_array, sizeof(T));
  return converted_value;
}
}  // namespace

extern "C" {
ATTRIBUTE_NO_SANITIZE_UNDEFINED
int LLVMFuzzerTestOneInput(unsigned char* data, size_t size) {
  if (size >= sizeof(pti_view_kind)) {
    auto view_type = ConvertByteArray<pti_view_kind>(data);
    ptiViewEnable(view_type);
  }

  pti_view_record_base* record = nullptr;
  ptiViewGetNextRecord(data, size, &record);

  if (size >= sizeof(pti_view_kind)) {
    auto view_type = ConvertByteArray<pti_view_kind>(data);
    ptiViewDisable(view_type);
  }
  return 0;
}
}

#ifndef TEST_UTILS_GTEST_HELPERS_H_
#define TEST_UTILS_GTEST_HELPERS_H_

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#define ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(X)                          \
  do {                                                                     \
    auto result = (X);                                                     \
    if (result == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE ||                   \
        result == ZE_RESULT_ERROR_UNSUPPORTED_VERSION) {                   \
      GTEST_SKIP() << "Skipping test due to unsupported driver call: " #X; \
    }                                                                      \
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);                                  \
  } while (0)

#endif  // TEST_UTILS_GTEST_HELPERS_H_

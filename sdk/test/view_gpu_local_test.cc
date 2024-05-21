#include <gtest/gtest.h>

#include <iostream>

#include "pti/pti_view.h"

class ViewGPULocalFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// at the moment at least execute this API
// TODO: more tests to be added;
// the will respect the version of L0 installed to the system
TEST_F(ViewGPULocalFixtureTest, CheckGPULocalViewAvailable) {
  pti_result result = ptiViewGPULocalAvailable();
  EXPECT_TRUE(result == pti_result::PTI_SUCCESS ||
              result == pti_result::PTI_ERROR_L0_LOCAL_PROFILING_NOT_SUPPORTED);

  std::cout << "result: " << (uint32_t)result << std::endl;

  // just exercising two times and verifying the answer is the same
  pti_result result2 = ptiViewGPULocalAvailable();
  EXPECT_TRUE(result2 == result);
}

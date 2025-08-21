#include <gtest/gtest.h>
#include <view_handler.h>

#include <cstring>

class AssertExceptionFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftUFValid) {
  Instance().SetState(pti_result::PTI_SUCCESS);
  uint64_t aTS = ApplyTimeShift(10, -10);
  ASSERT_EQ(aTS, 0ULL);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftUnderflow) {
  Instance().SetState(pti_result::PTI_SUCCESS);
  uint64_t aTS = ApplyTimeShift(0, -1);
  ASSERT_EQ(aTS, 0ULL);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftOFValid) {
  Instance().SetState(pti_result::PTI_SUCCESS);
  uint64_t aTS = ApplyTimeShift(UINT64_MAX - 1, 1);
  ASSERT_EQ(aTS, UINT64_MAX);
}

TEST_F(AssertExceptionFixtureTest, AssertApplyTimeShiftOverflow) {
  Instance().SetState(pti_result::PTI_SUCCESS);
  uint64_t aTS = ApplyTimeShift(UINT64_MAX, 1);
  ASSERT_EQ(aTS, 0ULL);
}

TEST_F(AssertExceptionFixtureTest, ExceptionGetViewNameAndCallbackThrow) {
  Instance().SetState(pti_result::PTI_SUCCESS);
  EXPECT_THROW(
      { GetViewNameAndCallback(pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION); }, std::out_of_range);
}

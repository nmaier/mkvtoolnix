#include "common/common_pch.h"

#include "common/option_with_source.h"

#include "gtest/gtest.h"

namespace {

TEST(OptionWithSource, CreationWithoutValue) {
  option_with_source_c<int64_t> o;

  ASSERT_EQ(static_cast<bool>(o), false);
  ASSERT_THROW(o.get(),           std::logic_error);
  ASSERT_THROW(o.get_source(),    std::logic_error);
}

TEST(OptionWithSource, CreationWithValue) {
  option_with_source_c<int64_t> o(42, OPTION_SOURCE_CONTAINER);

  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              42);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
}

TEST(OptionWithSource, SetValueOnce) {
  option_with_source_c<int64_t> o;

  ASSERT_EQ(static_cast<bool>(o), false);
  o.set(42, OPTION_SOURCE_CONTAINER);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              42);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
}

TEST(OptionWithSource, SetValueLowerSource) {
  option_with_source_c<int64_t> o;

  ASSERT_EQ(static_cast<bool>(o), false);
  o.set(42, OPTION_SOURCE_CONTAINER);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              42);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
  o.set(54, OPTION_SOURCE_BITSTREAM);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              42);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
}

TEST(OptionWithSource, SetValueSameSource) {
  option_with_source_c<int64_t> o;

  ASSERT_EQ(static_cast<bool>(o), false);
  o.set(42, OPTION_SOURCE_CONTAINER);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              42);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
  o.set(54, OPTION_SOURCE_CONTAINER);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              54);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
}

TEST(OptionWithSource, SetValueHigherSource) {
  option_with_source_c<int64_t> o;

  ASSERT_EQ(static_cast<bool>(o), false);
  o.set(42, OPTION_SOURCE_CONTAINER);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              42);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_CONTAINER);
  o.set(54, OPTION_SOURCE_COMMAND_LINE);
  ASSERT_EQ(static_cast<bool>(o), true);
  ASSERT_EQ(o.get(),              54);
  ASSERT_EQ(o.get_source(),       OPTION_SOURCE_COMMAND_LINE);
}

}

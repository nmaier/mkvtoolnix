#include "common/common_pch.h"

#include "common/strings/formatting.h"

#include "gtest/gtest.h"

namespace {

TEST(StringsFormatting, FileSize) {
  EXPECT_EQ(format_file_size(      1023ll), "1023 bytes");
  EXPECT_EQ(format_file_size(      1024ll), "1.0 KiB");
  EXPECT_EQ(format_file_size(      2047ll), "1.9 KiB");
  EXPECT_EQ(format_file_size(      2048ll), "2.0 KiB");
  EXPECT_EQ(format_file_size(   1048575ll), "1023.9 KiB");
  EXPECT_EQ(format_file_size(   1048576ll), "1.0 MiB");
  EXPECT_EQ(format_file_size(   2097151ll), "1.9 MiB");
  EXPECT_EQ(format_file_size(   2097152ll), "2.0 MiB");
  EXPECT_EQ(format_file_size(1073741823ll), "1023.9 MiB");
  EXPECT_EQ(format_file_size(1073741824ll), "1.0 GiB");
  EXPECT_EQ(format_file_size(2147483647ll), "1.9 GiB");
  EXPECT_EQ(format_file_size(2147483648ll), "2.0 GiB");
}

}

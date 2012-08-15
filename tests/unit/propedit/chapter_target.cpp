#include "common/common_pch.h"

#include "propedit/chapter_target.h"

#include "gtest/gtest.h"

namespace {

TEST(ChapterTarget, Basics) {
  chapter_target_c ct;

  EXPECT_TRUE(ct.has_changes());
}


}

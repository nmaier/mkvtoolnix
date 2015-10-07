#include "common/common_pch.h"

#include "gtest/gtest.h"
#include "tests/unit/util.h"

#include "common/mm_io_x.h"

namespace {

TEST(MmIo, Slurp) {
  memory_cptr m;

  ASSERT_NO_THROW(m = mm_file_io_c::slurp("tests/unit/data/text/chunky_bacon.txt"));
  EXPECT_EQ(*m, std::string{"Chunky Bacon\n"});

  ASSERT_THROW(mm_file_io_c::slurp("doesnotexist"), mtx::mm_io::exception);
}

}

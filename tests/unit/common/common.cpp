#include "common/common_pch.h"

#include "tests/unit/init.h"

int
main(int argc,
     char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ::mtxut::init_suite();
  return RUN_ALL_TESTS();
}

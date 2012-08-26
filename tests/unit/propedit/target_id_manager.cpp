#include "common/common_pch.h"

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>

#include "common/strings/utf8.h"
#include "propedit/target_id_manager.h"

#include "gtest/gtest.h"
#include "tests/unit/construct.h"
#include "tests/unit/util.h"

using namespace libmatroska;
using namespace mtxut::construct;

namespace {

TEST(TargetIdManager, Basics) {
  auto master = cons<KaxAttachments>(cons<KaxAttached>(),
                                     cons<KaxAttachments>(),
                                     cons<KaxAttachments>(),
                                     cons<KaxAttached>(),
                                     cons<KaxAttached>(),
                                     cons<KaxAttachments>());

  target_id_manager_c<KaxAttached> mgr(master, 1);

  EXPECT_FALSE(mgr.has(0));
  EXPECT_TRUE(mgr.has(1));
  EXPECT_TRUE(mgr.has(2));
  EXPECT_TRUE(mgr.has(3));
  EXPECT_FALSE(mgr.has(4));

  EXPECT_EQ(mgr.get(0), nullptr);
  EXPECT_EQ(mgr.get(1), (*master)[0]);
  EXPECT_EQ(mgr.get(2), (*master)[3]);
  EXPECT_EQ(mgr.get(3), (*master)[4]);
  EXPECT_EQ(mgr.get(4), nullptr);

  mgr.remove(2);

  EXPECT_FALSE(mgr.has(0));
  EXPECT_TRUE(mgr.has(1));
  EXPECT_FALSE(mgr.has(2));
  EXPECT_TRUE(mgr.has(3));
  EXPECT_FALSE(mgr.has(4));

  EXPECT_EQ(mgr.get(0), nullptr);
  EXPECT_EQ(mgr.get(1), (*master)[0]);
  EXPECT_EQ(mgr.get(2), nullptr);
  EXPECT_EQ(mgr.get(3), (*master)[4]);
  EXPECT_EQ(mgr.get(4), nullptr);
}

}

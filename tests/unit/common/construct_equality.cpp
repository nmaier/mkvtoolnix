#include "common/common_pch.h"

#include "gtest/gtest.h"
#include "common/construct.h"
#include "common/ebml.h"

#include "tests/unit/util.h"

#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackVideo.h>

namespace {

using namespace mtxut;
using namespace mtx::construct;
using namespace libmatroska;

TEST(ConstructAndEquality, EmptyMaster) {
  auto a = ebml_master_cptr{ cons<KaxTracks>() };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  EXPECT_EQ(a->ListSize(), 0u);
  EXPECT_EQ(b->ListSize(), 0u);
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, SingleLevel) {
  auto a = ebml_master_cptr{ cons<KaxTrackVideo>(new KaxVideoPixelWidth, 123, new KaxVideoPixelHeight, 456) };
  auto b = ebml_master_cptr{ master<KaxTrackVideo>() };
  b->PushElement(*new KaxVideoPixelWidth);
  b->PushElement(*new KaxVideoPixelHeight);
  *dynamic_cast<EbmlUInteger *>((*b)[0]) = 123;
  *dynamic_cast<EbmlUInteger *>((*b)[1]) = 456;
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, MultipleLevels) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(cons<KaxTrackVideo>())) };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  b->PushElement(*master<KaxTrackEntry>());
  static_cast<EbmlMaster *>((*b)[0])->PushElement(*master<KaxTrackVideo>());
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, MasterAtTheFront) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(), new KaxCodecID, std::string{"Stuff"}) };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  b->PushElement(*master<KaxTrackEntry>());
  b->PushElement(*new KaxCodecID);
  *static_cast<EbmlString *>((*b)[1]) = "Stuff";
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, MasterInTheMiddle) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(new KaxTrackUID, 4711u, cons<KaxTrackEntry>(), new KaxTrackUID, 4712u) };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  b->PushElement(*new KaxTrackUID);
  b->PushElement(*master<KaxTrackEntry>());
  b->PushElement(*new KaxTrackUID);
  *static_cast<EbmlUInteger *>((*b)[0]) = 4711;
  *static_cast<EbmlUInteger *>((*b)[2]) = 4712;
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, MasterAtTheBack) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(new KaxCodecID, "Stuff", cons<KaxTrackEntry>()) };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  b->PushElement(*new KaxCodecID);
  b->PushElement(*master<KaxTrackEntry>());
  *static_cast<EbmlString *>((*b)[0]) = "Stuff";
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, NoMaster) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(new KaxCodecID, "Stuff1", new KaxCodecID, "Stuff2") };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  b->PushElement(*new KaxCodecID);
  b->PushElement(*new KaxCodecID);
  *static_cast<EbmlString *>((*b)[0]) = "Stuff1";
  *static_cast<EbmlString *>((*b)[1]) = "Stuff2";
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, AllTypes) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(new KaxCodecID,     "Stuff",
                                                                 new KaxCodecName,   L"UniStuffAsWcharString",
                                                                 new KaxCodecName,   std::wstring{L"UniStuffAsStdWString"},
                                                                 new KaxCodecName,   std::string{"UniStuffAsStdString"},
                                                                 new KaxCodecName,   "UniStuffAsCharString",
                                                                 new KaxTrackNumber,  4254,
                                                                 new KaxTrackOffset,   -22,
                                                                 new KaxDateUTC,     98273,
                                                                 new KaxDuration,    47.11,
                                                                 cons<KaxTrackVideo>(new KaxVideoPixelWidth,  123,
                                                                                     new KaxVideoPixelHeight, 456)))
  };

  auto b = ebml_master_cptr{ master<KaxTracks>() };
  auto m1 = master<KaxTrackEntry>();
  auto m2 = master<KaxTrackVideo>();

  b->PushElement(*m1);

  m1->PushElement(*new KaxCodecID);
  m1->PushElement(*new KaxCodecName);
  m1->PushElement(*new KaxCodecName);
  m1->PushElement(*new KaxCodecName);
  m1->PushElement(*new KaxCodecName);
  m1->PushElement(*new KaxTrackNumber);
  m1->PushElement(*new KaxTrackOffset);
  m1->PushElement(*new KaxDateUTC);
  m1->PushElement(*new KaxDuration);
  m1->PushElement(*m2);

  m2->PushElement(*new KaxVideoPixelWidth);
  m2->PushElement(*new KaxVideoPixelHeight);

  *dynamic_cast<EbmlString *>((*m1)[0])        = "Stuff";
  *dynamic_cast<EbmlUnicodeString *>((*m1)[1]) = L"UniStuffAsWcharString";
  *dynamic_cast<EbmlUnicodeString *>((*m1)[2]) = L"UniStuffAsStdWString";
  *dynamic_cast<EbmlUnicodeString *>((*m1)[3]) = L"UniStuffAsStdString";
  *dynamic_cast<EbmlUnicodeString *>((*m1)[4]) = L"UniStuffAsCharString";
  *dynamic_cast<EbmlUInteger *>((*m1)[5])      = 4254;
  *dynamic_cast<EbmlSInteger *>((*m1)[6])      = -22;
   dynamic_cast<EbmlDate *>((*m1)[7])->SetEpochDate(98273);
  *dynamic_cast<EbmlFloat *>((*m1)[8])          = 47.11;

  *dynamic_cast<EbmlUInteger *>((*m2)[0])      = 123;
  *dynamic_cast<EbmlUInteger *>((*m2)[1])      = 456;

  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, Binary) {
  auto a = ebml_master_cptr{ cons<KaxTracks>(new KaxCodecPrivate, memory_c::clone("Stuff", 5)) };
  auto b = ebml_master_cptr{ master<KaxTracks>() };
  b->PushElement(*new KaxCodecPrivate);
  static_cast<EbmlBinary *>((*b)[0])->CopyBuffer(reinterpret_cast<binary const *>("Stuff"), 5);
  EXPECT_EBML_EQ(*a, *b);
}

TEST(ConstructAndEquality, ConditionalConstruction) {
  bool hmm = false;
  auto a = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(new KaxCodecID,                      "Stuff",
                                                                 hmm ? new KaxTrackNumber : nullptr,  4254,
                                                                 new KaxTrackOffset,                   -22))
  };

  auto b = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(new KaxCodecID,                      "Stuff",
                                                                 new KaxTrackOffset,                   -22))
  };

  EXPECT_EBML_EQ(*a, *b);

  hmm = true;
  auto c = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(new KaxCodecID,                      "Stuff",
                                                                 hmm ? new KaxTrackNumber : nullptr,  4254,
                                                                 new KaxTrackOffset,                   -22))
  };

  auto d = ebml_master_cptr{ cons<KaxTracks>(cons<KaxTrackEntry>(new KaxCodecID,                      "Stuff",
                                                                 new KaxTrackNumber,                  4254,
                                                                 new KaxTrackOffset,                   -22))
  };

  EXPECT_EBML_EQ(*c, *d);
}

}

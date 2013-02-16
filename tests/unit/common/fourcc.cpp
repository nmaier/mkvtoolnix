#include "common/common_pch.h"

#include <sstream>

#include "common/endian.h"
#include "common/fourcc.h"

#include "gtest/gtest.h"

namespace {

uint32_t big    = 0x31323334;
uint32_t little = 0x34333231;

TEST(FourCC, CreationFromBigUInt32) {
  EXPECT_EQ(big, fourcc_c(big)                      .value());
  EXPECT_EQ(big, fourcc_c(big, fourcc_c::big_endian).value());
  EXPECT_EQ(big, fourcc_c(big)                      .value(fourcc_c::big_endian));
  EXPECT_EQ(big, fourcc_c(big, fourcc_c::big_endian).value(fourcc_c::big_endian));

  unsigned char buffer[4];
  put_uint32_be(buffer, big);
  EXPECT_EQ(big, fourcc_c(reinterpret_cast<uint32_t *>(buffer))                      .value());
  EXPECT_EQ(big, fourcc_c(reinterpret_cast<uint32_t *>(buffer), fourcc_c::big_endian).value());
  EXPECT_EQ(big, fourcc_c(reinterpret_cast<uint32_t *>(buffer))                      .value(fourcc_c::big_endian));
  EXPECT_EQ(big, fourcc_c(reinterpret_cast<uint32_t *>(buffer), fourcc_c::big_endian).value(fourcc_c::big_endian));
}

TEST(FourCC, CreationFromLittleUInt32) {
  EXPECT_EQ(little, fourcc_c(little)                         .value());
  EXPECT_EQ(big,    fourcc_c(little, fourcc_c::little_endian).value());
  EXPECT_EQ(big,    fourcc_c(little)                         .value(fourcc_c::little_endian));
  EXPECT_EQ(little, fourcc_c(little, fourcc_c::little_endian).value(fourcc_c::little_endian));
}

TEST(FourCC, CreationFromStrings) {
  std::string big_s    = "1234";
  std::string little_s = "4321";

  EXPECT_EQ(big,    fourcc_c(big_s)   .value());
  EXPECT_EQ(little, fourcc_c(little_s).value());
  EXPECT_EQ(big,    fourcc_c("1234")  .value());
  EXPECT_EQ(little, fourcc_c("4321")  .value());
}

TEST(FourCC, CreationFromMemoryC) {
  auto big_m    = memory_c::alloc(4);
  auto little_m = memory_c::alloc(4);

  put_uint32_be(big_m->get_buffer(),    big);
  put_uint32_le(little_m->get_buffer(), big);

  EXPECT_EQ(big,    fourcc_c(big_m)                            .value());
  EXPECT_EQ(big,    fourcc_c(big_m,    fourcc_c::big_endian)   .value());
  EXPECT_EQ(big,    fourcc_c(big_m)                            .value(fourcc_c::big_endian));
  EXPECT_EQ(big,    fourcc_c(big_m,    fourcc_c::big_endian)   .value(fourcc_c::big_endian));
  EXPECT_EQ(little, fourcc_c(little_m)                         .value());
  EXPECT_EQ(big,    fourcc_c(little_m, fourcc_c::little_endian).value());
  EXPECT_EQ(big,    fourcc_c(little_m)                         .value(fourcc_c::little_endian));
  EXPECT_EQ(little, fourcc_c(little_m, fourcc_c::little_endian).value(fourcc_c::little_endian));
}

TEST(FourCC, CreationFromUnsignedCharPtr) {
  unsigned char big_m[4], little_m[4];

  put_uint32_be(big_m,    big);
  put_uint32_le(little_m, big);

  EXPECT_EQ(big,    fourcc_c(big_m)                            .value());
  EXPECT_EQ(big,    fourcc_c(big_m,    fourcc_c::big_endian)   .value());
  EXPECT_EQ(big,    fourcc_c(big_m)                            .value(fourcc_c::big_endian));
  EXPECT_EQ(big,    fourcc_c(big_m,    fourcc_c::big_endian)   .value(fourcc_c::big_endian));
  EXPECT_EQ(little, fourcc_c(little_m)                         .value());
  EXPECT_EQ(big,    fourcc_c(little_m, fourcc_c::little_endian).value());
  EXPECT_EQ(big,    fourcc_c(little_m)                         .value(fourcc_c::little_endian));
  EXPECT_EQ(little, fourcc_c(little_m, fourcc_c::little_endian).value(fourcc_c::little_endian));
}

TEST(FourCC, CreationFromMmIo) {
  unsigned char big_m[4], little_m[4];

  put_uint32_be(big_m,    big);
  put_uint32_le(little_m, big);

  EXPECT_EQ(big,    fourcc_c(mm_io_cptr{new mm_mem_io_c{big_m,    4}})                               .value());
  EXPECT_EQ(big,    fourcc_c(mm_io_cptr{new mm_mem_io_c{big_m,    4}},       fourcc_c::big_endian)   .value());
  EXPECT_EQ(big,    fourcc_c(mm_io_cptr{new mm_mem_io_c{big_m,    4}})                               .value(fourcc_c::big_endian));
  EXPECT_EQ(big,    fourcc_c(mm_io_cptr{new mm_mem_io_c{big_m,    4}},       fourcc_c::big_endian)   .value(fourcc_c::big_endian));
  EXPECT_EQ(little, fourcc_c(mm_io_cptr{new mm_mem_io_c{little_m, 4}})                               .value());
  EXPECT_EQ(big,    fourcc_c(mm_io_cptr{new mm_mem_io_c{little_m, 4}},       fourcc_c::little_endian).value());
  EXPECT_EQ(big,    fourcc_c(mm_io_cptr{new mm_mem_io_c{little_m, 4}})                               .value(fourcc_c::little_endian));
  EXPECT_EQ(little, fourcc_c(mm_io_cptr{new mm_mem_io_c{little_m, 4}},       fourcc_c::little_endian).value(fourcc_c::little_endian));

  EXPECT_EQ(big,    fourcc_c(           new mm_mem_io_c{big_m,    4})                                .value());
  EXPECT_EQ(big,    fourcc_c(           new mm_mem_io_c{big_m,    4},        fourcc_c::big_endian)   .value());
  EXPECT_EQ(big,    fourcc_c(           new mm_mem_io_c{big_m,    4})                                .value(fourcc_c::big_endian));
  EXPECT_EQ(big,    fourcc_c(           new mm_mem_io_c{big_m,    4},        fourcc_c::big_endian)   .value(fourcc_c::big_endian));
  EXPECT_EQ(little, fourcc_c(           new mm_mem_io_c{little_m, 4})                                .value());
  EXPECT_EQ(big,    fourcc_c(           new mm_mem_io_c{little_m, 4},        fourcc_c::little_endian).value());
  EXPECT_EQ(big,    fourcc_c(           new mm_mem_io_c{little_m, 4})                                .value(fourcc_c::little_endian));
  EXPECT_EQ(little, fourcc_c(           new mm_mem_io_c{little_m, 4},        fourcc_c::little_endian).value(fourcc_c::little_endian));

  auto big_io    = mm_io_cptr{new mm_mem_io_c{big_m,    4}};
  auto little_io = mm_io_cptr{new mm_mem_io_c{little_m, 4}};

  big_io->setFilePointer(0);    EXPECT_EQ(big,    fourcc_c(big_io.get())                            .value());
  big_io->setFilePointer(0);    EXPECT_EQ(big,    fourcc_c(big_io.get(),    fourcc_c::big_endian)   .value());
  big_io->setFilePointer(0);    EXPECT_EQ(big,    fourcc_c(big_io.get())                            .value(fourcc_c::big_endian));
  big_io->setFilePointer(0);    EXPECT_EQ(big,    fourcc_c(big_io.get(),    fourcc_c::big_endian)   .value(fourcc_c::big_endian));
  little_io->setFilePointer(0); EXPECT_EQ(little, fourcc_c(little_io.get())                         .value());
  little_io->setFilePointer(0); EXPECT_EQ(big,    fourcc_c(little_io.get(), fourcc_c::little_endian).value());
  little_io->setFilePointer(0); EXPECT_EQ(big,    fourcc_c(little_io.get())                         .value(fourcc_c::little_endian));
  little_io->setFilePointer(0); EXPECT_EQ(little, fourcc_c(little_io.get(), fourcc_c::little_endian).value(fourcc_c::little_endian));
}

TEST(FourCC, Equality) {
  fourcc_c big_f{big};
  fourcc_c little_f{little, fourcc_c::little_endian};

  fourcc_c big_bad_f{big + 1};
  fourcc_c little_bad_f{little + 1, fourcc_c::little_endian};

  // EXPECT_TRUE(big_f         == big);
  // EXPECT_TRUE(little_f      == big);

  EXPECT_TRUE(big_f         == "1234");
  EXPECT_TRUE(little_f      == "1234");

  EXPECT_FALSE(big_bad_f    == big);
  EXPECT_FALSE(little_bad_f == big);

  EXPECT_FALSE(big_bad_f    == "1234");
  EXPECT_FALSE(little_bad_f == "1234");

  EXPECT_FALSE(big_f        != big);
  EXPECT_FALSE(little_f     != big);

  EXPECT_FALSE(big_f        != "1234");
  EXPECT_FALSE(little_f     != "1234");

  EXPECT_TRUE(big_bad_f     != big);
  EXPECT_TRUE(little_bad_f  != big);

  EXPECT_TRUE(big_bad_f     != "1234");
  EXPECT_TRUE(little_bad_f  != "1234");
}

TEST(FourCC, Stringification) {
  fourcc_c big_f{big};

  EXPECT_EQ(big_f.str(), "1234");

  std::stringstream sstr;
  EXPECT_NO_THROW(sstr << big_f);
  EXPECT_EQ(sstr.str(), "1234");

  EXPECT_EQ(fourcc_c{}.str(),           "????");
  EXPECT_EQ(fourcc_c{0x31003200}.str(), "1?2?");
}

TEST(FourCC, WritingToMemory) {
  unsigned char buf[4];
  auto mem = memory_c::alloc(4);

  fourcc_c big_f{big}, little_f{little, fourcc_c::little_endian};

  ASSERT_EQ(big_f, little_f);

  memset(mem->get_buffer(), 0, 4);
  EXPECT_EQ(4u, big_f.write(mem));
  EXPECT_EQ(big, get_uint32_be(mem->get_buffer()));

  memset(mem->get_buffer(), 0, 4);
  EXPECT_EQ(4u, big_f.write(mem, fourcc_c::big_endian));
  EXPECT_EQ(big, get_uint32_be(mem->get_buffer()));

  memset(mem->get_buffer(), 0, 4);
  EXPECT_EQ(4u, big_f.write(mem, fourcc_c::little_endian));
  EXPECT_EQ(little, get_uint32_be(mem->get_buffer()));

  memset(buf, 0, 4);
  EXPECT_EQ(4u, big_f.write(buf));
  EXPECT_EQ(big, get_uint32_be(buf));

  memset(buf, 0, 4);
  EXPECT_EQ(4u, big_f.write(buf, fourcc_c::big_endian));
  EXPECT_EQ(big, get_uint32_be(buf));

  memset(buf, 0, 4);
  EXPECT_EQ(4u, big_f.write(buf, fourcc_c::little_endian));
  EXPECT_EQ(little, get_uint32_be(buf));

  memset(mem->get_buffer(), 0, 4);
  EXPECT_EQ(4u, little_f.write(mem));
  EXPECT_EQ(big, get_uint32_be(mem->get_buffer()));

  memset(mem->get_buffer(), 0, 4);
  EXPECT_EQ(4u, little_f.write(mem, fourcc_c::big_endian));
  EXPECT_EQ(big, get_uint32_be(mem->get_buffer()));

  memset(mem->get_buffer(), 0, 4);
  EXPECT_EQ(4u, little_f.write(mem, fourcc_c::little_endian));
  EXPECT_EQ(little, get_uint32_be(mem->get_buffer()));

  memset(buf, 0, 4);
  EXPECT_EQ(4u, little_f.write(buf));
  EXPECT_EQ(big, get_uint32_be(buf));

  memset(buf, 0, 4);
  EXPECT_EQ(4u, little_f.write(buf, fourcc_c::big_endian));
  EXPECT_EQ(big, get_uint32_be(buf));

  memset(buf, 0, 4);
  EXPECT_EQ(4u, little_f.write(buf, fourcc_c::little_endian));
  EXPECT_EQ(little, get_uint32_be(buf));
}

TEST(FourCC, WritingToMmIo) {
  fourcc_c big_f{big}, little_f{little, fourcc_c::little_endian};

  auto m1_1 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 4}};
  EXPECT_EQ(4u, big_f.write(m1_1));
  EXPECT_EQ(4u, m1_1->get_size());
  EXPECT_NO_THROW(m1_1->setFilePointer(0));
  EXPECT_EQ(big, m1_1->read_uint32_be());

  auto m1_2 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 1}};
  EXPECT_EQ(4u, big_f.write(m1_2, fourcc_c::big_endian));
  EXPECT_EQ(4u, m1_2->get_size());
  EXPECT_NO_THROW(m1_2->setFilePointer(0));
  EXPECT_EQ(big, m1_2->read_uint32_be());

  auto m1_3 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 1}};
  EXPECT_EQ(4u, big_f.write(m1_3, fourcc_c::little_endian));
  EXPECT_EQ(4u, m1_3->get_size());
  EXPECT_NO_THROW(m1_3->setFilePointer(0));
  EXPECT_EQ(little, m1_3->read_uint32_be());

  auto m2_1 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 4}};
  EXPECT_EQ(4u, big_f.write(*m2_1));
  EXPECT_EQ(4u, m2_1->get_size());
  EXPECT_NO_THROW(m2_1->setFilePointer(0));
  EXPECT_EQ(big, m2_1->read_uint32_be());

  auto m2_2 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 1}};
  EXPECT_EQ(4u, big_f.write(*m2_2, fourcc_c::big_endian));
  EXPECT_EQ(4u, m2_2->get_size());
  EXPECT_NO_THROW(m2_2->setFilePointer(0));
  EXPECT_EQ(big, m2_2->read_uint32_be());

  auto m2_3 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 1}};
  EXPECT_EQ(4u, big_f.write(*m2_3, fourcc_c::little_endian));
  EXPECT_EQ(4u, m2_3->get_size());
  EXPECT_NO_THROW(m2_3->setFilePointer(0));
  EXPECT_EQ(little, m2_3->read_uint32_be());

  auto m3_1 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 4}};
  EXPECT_EQ(4u, big_f.write(m3_1.get()));
  EXPECT_EQ(4u, m3_1->get_size());
  EXPECT_NO_THROW(m3_1->setFilePointer(0));
  EXPECT_EQ(big, m3_1->read_uint32_be());

  auto m3_2 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 1}};
  EXPECT_EQ(4u, big_f.write(m3_2.get(), fourcc_c::big_endian));
  EXPECT_EQ(4u, m3_2->get_size());
  EXPECT_NO_THROW(m3_2->setFilePointer(0));
  EXPECT_EQ(big, m3_2->read_uint32_be());

  auto m3_3 = mm_io_cptr{new mm_mem_io_c{nullptr, 4, 1}};
  EXPECT_EQ(4u, big_f.write(m3_3.get(), fourcc_c::little_endian));
  EXPECT_EQ(4u, m3_3->get_size());
  EXPECT_NO_THROW(m3_3->setFilePointer(0));
  EXPECT_EQ(little, m3_3->read_uint32_be());
}

TEST(FourCC, Resetting) {
  EXPECT_EQ(0u,  fourcc_c{}.value());
  EXPECT_EQ(big, fourcc_c{big}.value());
  EXPECT_EQ(0u,  fourcc_c{big}.reset().value());

  fourcc_c big_f{big};
  ASSERT_EQ(big, big_f.value());
  big_f.reset();
  EXPECT_EQ(0u,  big_f.value());
}

TEST(FourCC, OperatorBool) {
  EXPECT_TRUE(bool(fourcc_c{big}));
  EXPECT_TRUE(!fourcc_c{});

  EXPECT_FALSE(!fourcc_c{big});
  EXPECT_FALSE(bool(fourcc_c{}));
}

TEST(FourCC, Equivalence) {
  EXPECT_TRUE(fourcc_c{"ABCD"}.equiv("abcd"));
  EXPECT_TRUE(fourcc_c{"ABcd"}.equiv("abCD"));
  EXPECT_TRUE(fourcc_c{"abcd"}.equiv("ABCD"));
  EXPECT_TRUE(fourcc_c{"ABCD"}.equiv("ABCD"));

  EXPECT_FALSE(fourcc_c{"ABCD"}.equiv("qwer"));
  EXPECT_FALSE(fourcc_c{"abcd"}.equiv("qwer"));

  std::vector<std::string> vec{ "abcd", "WeRt", "uiOP", "VBNM" };
  EXPECT_TRUE(fourcc_c{"ABCD"}.equiv(vec));
  EXPECT_TRUE(fourcc_c{"WeRt"}.equiv(vec));
  EXPECT_TRUE(fourcc_c{"vbnm"}.equiv(vec));

  EXPECT_FALSE(fourcc_c{"dcba"}.equiv(vec));
}

TEST(FourCC, HumanReadable) {
  EXPECT_TRUE(fourcc_c{"ABCD"}.human_readable());
  EXPECT_TRUE(fourcc_c{"ABC?"}.human_readable());
  EXPECT_TRUE(fourcc_c{"AB??"}.human_readable(2));

  EXPECT_TRUE(fourcc_c{"abcd"}.human_readable());
  EXPECT_TRUE(fourcc_c{"abc?"}.human_readable());
  EXPECT_TRUE(fourcc_c{"ab??"}.human_readable(2));

  EXPECT_TRUE(fourcc_c{"a007"}.human_readable());
  EXPECT_TRUE(fourcc_c{"?007"}.human_readable());

  EXPECT_FALSE(fourcc_c{"abc?"}.human_readable(4));
  EXPECT_FALSE(fourcc_c{"ab??"}.human_readable());

  EXPECT_FALSE(fourcc_c{"abc?"}.human_readable(4));
  EXPECT_FALSE(fourcc_c{"ab??"}.human_readable());

  EXPECT_FALSE(fourcc_c{"??07"}.human_readable());

  EXPECT_TRUE(fourcc_c{0xa9a9a9a9}.human_readable());
}

}

#include "common/common_pch.h"

#include "common/timecode.h"

#include "gtest/gtest.h"

namespace {

TEST(BasicTimecode, Creation) {
  EXPECT_EQ(            1ll, timecode_c::factor(1).to_ns());
  EXPECT_EQ(            1ll, timecode_c::ns(1).to_ns());
  EXPECT_EQ(         1000ll, timecode_c::us(1).to_ns());
  EXPECT_EQ(      1000000ll, timecode_c::ms(1).to_ns());
  EXPECT_EQ(   1000000000ll, timecode_c::s(1).to_ns());
  EXPECT_EQ(  60000000000ll, timecode_c::m(1).to_ns());
  EXPECT_EQ(3600000000000ll, timecode_c::h(1).to_ns());
  EXPECT_EQ(        11111ll, timecode_c::mpeg(1).to_ns());
}

TEST(BasicTimecode, Deconstruction) {
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_ns(),    7204003002001ll);
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_us(),       7204003002ll);
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_ms(),          7204003ll);
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_s(),              7204ll);
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_m(),               120ll);
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_h(),                 2ll);
  EXPECT_EQ(timecode_c::ns(7204003002001ll).to_mpeg(),      648360270ll);
}

TEST(BasicTimecode, ArithmeticBothValid) {
  EXPECT_TRUE((timecode_c::s(2)        + timecode_c::us(500000)).valid());
  EXPECT_TRUE((timecode_c::s(2)        - timecode_c::us(500000)).valid());
  EXPECT_TRUE((timecode_c::us(1250000) * timecode_c::factor(2)).valid());
  EXPECT_TRUE((timecode_c::us(9900000) / timecode_c::factor(3)).valid());

  EXPECT_TRUE(timecode_c::s(-3).abs().valid());
}

TEST(BasicTimecode, ArithmenticResults) {
  EXPECT_EQ(timecode_c::ms(2500), timecode_c::s(2)        + timecode_c::us(500000));
  EXPECT_EQ(timecode_c::ms(1500), timecode_c::s(2)        - timecode_c::us(500000));
  EXPECT_EQ(timecode_c::ms(2500), timecode_c::us(1250000) * timecode_c::factor(2));
  EXPECT_EQ(timecode_c::ms(3300), timecode_c::us(9900000) / timecode_c::factor(3));

  EXPECT_EQ(timecode_c::s(-3).abs(), timecode_c::s(3));
  EXPECT_EQ(timecode_c::s(-3).abs(), timecode_c::s(3).abs());
}

TEST(BasicTimecode, ArithmenticLHSInvalid) {
  EXPECT_FALSE((timecode_c{} + timecode_c::m(2)).valid());
  EXPECT_FALSE((timecode_c{} - timecode_c::m(3)).valid());
  EXPECT_FALSE((timecode_c{} * timecode_c::factor(2)).valid());
  EXPECT_FALSE((timecode_c{} / timecode_c::factor(4)).valid());
}

TEST(BasicTimecode, ArithmenticRHSInvalid) {
  EXPECT_FALSE((timecode_c::m(2)      + timecode_c{}).valid());
  EXPECT_FALSE((timecode_c::m(3)      - timecode_c{}).valid());
  EXPECT_FALSE((timecode_c::factor(2) * timecode_c{}).valid());
  EXPECT_FALSE((timecode_c::factor(4) / timecode_c{}).valid());
}

TEST(BasicTimecode, ArithmenticBothInvalid) {
  EXPECT_FALSE((timecode_c{} + timecode_c{}).valid());
  EXPECT_FALSE((timecode_c{} - timecode_c{}).valid());
  EXPECT_FALSE((timecode_c{} * timecode_c{}).valid());
  EXPECT_FALSE((timecode_c{} / timecode_c{}).valid());
}

TEST(BasicTimecode, ComparisonBothValid) {
  EXPECT_TRUE( timecode_c::ms(2500) <  timecode_c::s(3));
  EXPECT_TRUE( timecode_c::ms(2500) <= timecode_c::s(3));
  EXPECT_FALSE(timecode_c::ms(2500) >  timecode_c::s(3));
  EXPECT_FALSE(timecode_c::ms(2500) >= timecode_c::s(3));
  EXPECT_TRUE( timecode_c::ms(2500) != timecode_c::s(3));
  EXPECT_FALSE(timecode_c::ms(2500) == timecode_c::s(3));

  EXPECT_FALSE(timecode_c::ms(3000) <  timecode_c::s(3));
  EXPECT_TRUE( timecode_c::ms(3000) <= timecode_c::s(3));
  EXPECT_FALSE(timecode_c::ms(3000) >  timecode_c::s(3));
  EXPECT_TRUE( timecode_c::ms(3000) >= timecode_c::s(3));
  EXPECT_FALSE(timecode_c::ms(3000) != timecode_c::s(3));
  EXPECT_TRUE( timecode_c::ms(3000) == timecode_c::s(3));

  EXPECT_FALSE(timecode_c::ms(4000) <= timecode_c::s(3));
  EXPECT_TRUE( timecode_c::ms(4000) >  timecode_c::s(3));
}

TEST(BasicTimecode, ComparisonLHSInvalid) {
  EXPECT_TRUE( timecode_c{} <  timecode_c::ns(3));
  EXPECT_FALSE(timecode_c{} == timecode_c::ns(3));
}

TEST(BasicTimecode, ComparisonRHSInvalid) {
  EXPECT_FALSE(timecode_c::ns(3) <  timecode_c{});
  EXPECT_FALSE(timecode_c::ns(3) == timecode_c{});
}

TEST(BasicTimecode, ComparisonBothInvalid) {
  EXPECT_FALSE(timecode_c{} <  timecode_c{});
  EXPECT_TRUE( timecode_c{} == timecode_c{});
}

TEST(BasicTimecode, ThrowOnDeconstructionOfInvalid) {
  EXPECT_THROW(timecode_c{}.to_ns(),   std::domain_error);
  EXPECT_THROW(timecode_c{}.to_us(),   std::domain_error);
  EXPECT_THROW(timecode_c{}.to_ms(),   std::domain_error);
  EXPECT_THROW(timecode_c{}.to_s(),    std::domain_error);
  EXPECT_THROW(timecode_c{}.to_m(),    std::domain_error);
  EXPECT_THROW(timecode_c{}.to_h(),    std::domain_error);
  EXPECT_THROW(timecode_c{}.to_mpeg(), std::domain_error);

  EXPECT_NO_THROW(timecode_c{}.to_ns(1));
}

TEST(BasicTimecode, ConstructFromSamples) {
  EXPECT_EQ(timecode_c::samples(     0, 48000).to_ns(),          0);
  EXPECT_EQ(timecode_c::samples( 19999, 48000).to_ns(),  416645833);
  EXPECT_EQ(timecode_c::samples( 20000, 48000).to_ns(),  416666667);
  EXPECT_EQ(timecode_c::samples( 48000, 48000).to_ns(), 1000000000);
  EXPECT_EQ(timecode_c::samples(123456, 48000).to_ns(), 2572000000);

  EXPECT_THROW(timecode_c::samples(123, 0), std::domain_error);
}

TEST(BasicTimecode, DeconstructToSamples) {
  EXPECT_EQ(timecode_c::ns(         0).to_samples(48000),      0);
  EXPECT_EQ(timecode_c::ns( 416645833).to_samples(48000),  19999);
  EXPECT_EQ(timecode_c::ns( 416666667).to_samples(48000),  20000);
  EXPECT_EQ(timecode_c::ns(1000000000).to_samples(48000),  48000);
  EXPECT_EQ(timecode_c::ns(2572000000).to_samples(48000), 123456);

  EXPECT_THROW(timecode_c::ns(123).to_samples(0), std::domain_error);
}

TEST(BasicTimecode, Resetting) {
  auto v = timecode_c::ns(1);

  EXPECT_TRUE(v.valid());
  EXPECT_NO_THROW(v.to_ns());

  v.reset();
  EXPECT_FALSE(v.valid());
  EXPECT_THROW(v.to_ns(), std::domain_error);
}


}

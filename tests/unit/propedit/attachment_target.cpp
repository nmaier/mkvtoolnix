#include "common/common_pch.h"

#include "propedit/attachment_target.h"

#include "gtest/gtest.h"
#include "tests/unit/construct.h"
#include "tests/unit/util.h"

namespace {

TEST(AttachmentTarget, Basics) {
  attachment_target_c at;

  EXPECT_TRUE(at.has_changes());
}

TEST(AttachmentTarget, ParsingCorrectArgumentsForAdd) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_add, "bla.txt", no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_add, "bla:blubb.txt", no_opt));
}

TEST(AttachmentTarget, ParsingCorrectArgumentsForDelete) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "123123123",                    no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "=123123123",                   no_opt));

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "name:blubb",                   no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "mime-type:doesnotexist/blubb", no_opt));

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "NAME:blubb",                   no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "MImE-tYpe:doesnotexist/blubb", no_opt));
}

TEST(AttachmentTarget, ParsingCorrectArgumentsForReplace) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "123123123:bla.txt",                    no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "=123123123:bla.txt",                   no_opt));

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "name:blubb:bla.txt",                   no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "mime-type:doesnotexist/blubb:bla.txt", no_opt));

  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "NAME:blubb:bla.txt",                   no_opt));
  EXPECT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "MImE-tYpe:doesnotexist/blubb:bla.txt", no_opt));
}

TEST(AttachmentTarget, ParsingInvalidArgumentsForAdd) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_add, "", no_opt), std::invalid_argument);
}

TEST(AttachmentTarget, ParsingInvalidArgumentsForDelete) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "",            no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "=",           no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "-",           no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "=-",          no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "-123",        no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "=-123",       no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "123:",        no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "=123:",       no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "123:abc",     no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "=123:abc",    no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "name:",       no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "mime-type:",  no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_delete, "gonzo:peter", no_opt), std::invalid_argument);
}

TEST(AttachmentTarget, ParsingInvalidArgumentsForReplace) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "",                        no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "=",                       no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "-",                       no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "=-",                      no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "-123",                    no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "=-123",                   no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "123:",                    no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "=123:",                   no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "name:bla",                no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "mime-type:chunky/bacon",  no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "name:bla:",               no_opt), std::invalid_argument);
  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "mime-type:chunky/bacon:", no_opt), std::invalid_argument);

  EXPECT_THROW(at.parse_spec(attachment_target_c::ac_replace, "gonzo:peter:wuff",        no_opt), std::invalid_argument);
}

}

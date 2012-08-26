#include "common/common_pch.h"

#include "common/construct.h"
#include "common/strings/utf8.h"
#include "common/unique_numbers.h"
#include "propedit/attachment_target.h"

#include "gtest/gtest.h"
#include "tests/unit/init.h"
#include "tests/unit/util.h"

namespace {

using namespace mtx::construct;
using namespace mtxut;

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

TEST(AttachmentTarget, ValidateOk) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  ASSERT_NO_THROW(at.parse_spec(attachment_target_c::ac_delete, "1", no_opt));
  ASSERT_NO_THROW(at.validate());

  at = attachment_target_c{};

  ASSERT_NO_THROW(at.parse_spec(attachment_target_c::ac_add, "tests/unit/data/text/chunky_bacon.txt", no_opt));
  ASSERT_NO_THROW(at.validate());

  at = attachment_target_c{};

  ASSERT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "1:tests/unit/data/text/chunky_bacon.txt", no_opt));
  ASSERT_NO_THROW(at.validate());
}

TEST(AttachmentTarget, ValidateFailure) {
  attachment_target_c at;
  attachment_target_c::options_t no_opt;

  ASSERT_NO_THROW(at.parse_spec(attachment_target_c::ac_add, "doesnotexist", no_opt));
  ASSERT_THROW(at.validate(), mtxut::mxerror_x);

  at = attachment_target_c{};

  ASSERT_NO_THROW(at.parse_spec(attachment_target_c::ac_replace, "1:doesnotexist", no_opt));
  ASSERT_THROW(at.validate(), mtxut::mxerror_x);
}

EbmlMaster *
cons_att(std::string const &content,
         std::string const &name,
         std::string const &mime_type,
         uint64_t uid,
         std::string const &description = "") {
  return cons<KaxAttached>(new KaxFileName,                                         to_wide(name),
                           !description.empty() ? new KaxFileDescription : nullptr, to_wide(description),
                           new KaxMimeType,                                         mime_type,
                           new KaxFileUID,                                          uid,
                           new KaxFileData,                                         memory_c::clone(content.c_str(), content.length()));
}

EbmlMaster *
cons_default_atts() {
  return cons<KaxAttachments>(cons_att("Hello World\nThis is fun.\nChunky Bacon!!",        "Dummy File.txt",       "text/plain",                47110815, "Some funky description"),
                              cons_att("<html><body><h1>Chunky Bacon!</h2></body></html>", "chunky bacon.html",    "text/html",                 123454321),
                              cons_att("198273498725987195610824371289567129357",          "filename:with:colons", "application/otctet-stream", 99887766),
                              cons_att("<html><body><h1>Chunky Bacon!</h2></body></html>", "chunky bacon.html",    "text/html",                 918273645));
}

void
test_add(std::function<void(ebml_master_cptr &l1_b, attachment_target_c::options_t &opt)> local_setup) {
  clear_list_of_unique_numbers(UNIQUE_ALL_IDS);

  auto l1_a = ebml_element_cptr{cons_default_atts()};
  auto l1_b = ebml_master_cptr{cons_default_atts()};

  attachment_target_c at;
  attachment_target_c::options_t opt;

  local_setup(l1_b, opt);

  at.set_level1_element(l1_a);

  ASSERT_NO_THROW(at.parse_spec(attachment_target_c::ac_add, "tests/unit/data/text/chunky_bacon.txt", opt));
  ASSERT_NO_THROW(at.validate());
  ASSERT_NO_THROW(at.execute());
  EXPECT_EBML_EQ(*l1_a, *l1_b);
}

TEST(AttachmentTarget, AddDetectAll) {
  test_add([](ebml_master_cptr &l1_b, attachment_target_c::options_t &) {
      l1_b->PushElement(*cons_att("Chunky Bacon\n", "chunky_bacon.txt", "text/plain", 1));
    });
}

TEST(AttachmentTarget, AddProvideName) {
  test_add([](ebml_master_cptr &l1_b, attachment_target_c::options_t &opt) {
      opt.name("Charlie!");
      l1_b->PushElement(*cons_att("Chunky Bacon\n", "Charlie!", "text/plain", 1));
    });
}

TEST(AttachmentTarget, AddProvideMIMEType) {
  test_add([](ebml_master_cptr &l1_b, attachment_target_c::options_t &opt) {
      opt.mime_type("inva/lid");
      l1_b->PushElement(*cons_att("Chunky Bacon\n", "chunky_bacon.txt", "inva/lid", 1));
    });
}

TEST(AttachmentTarget, AddProvideDescription) {
  test_add([](ebml_master_cptr &l1_b, attachment_target_c::options_t &opt) {
      opt.description("somewhere...");
      l1_b->PushElement(*cons_att("Chunky Bacon\n", "chunky_bacon.txt", "text/plain", 1, "somewhere..."));
    });
}

TEST(AttachmentTarget, AddProvideAll) {
  test_add([](ebml_master_cptr &l1_b, attachment_target_c::options_t &opt) {
      opt.mime_type("empty/rooms").description("where we learn to live").name("stuffy.h");
      l1_b->PushElement(*cons_att("Chunky Bacon\n", "stuffy.h", "empty/rooms", 1, "where we learn to live"));
    });
}

}

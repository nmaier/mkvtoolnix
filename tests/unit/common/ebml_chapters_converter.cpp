#include "common/common_pch.h"

#include <ebml/EbmlVoid.h>

#include "gtest/gtest.h"
#include "tests/unit/init.h"
#include "tests/unit/util.h"

#include "common/xml/ebml_chapters_converter.h"

namespace {

TEST(EbmlChaptersConverter, ToXmlAndEbmlVoid) {
  KaxChapters chapters;
  chapters.PushElement(*new EbmlVoid);
  mm_mem_io_c mem_io{nullptr, 0, 1000};

  ASSERT_NO_THROW(mtx::xml::ebml_chapters_converter_c::write_xml(chapters, mem_io));

  std::string const expected_output("\xEF\xBB\xBF<?xml version=\"1.0\"?>\n<!-- <!DOCTYPE Chapters SYSTEM \"matroskachapters.dtd\"> -->\n<Chapters />\n");
  auto actual_output = mem_io.get_content();

  ASSERT_EQ(actual_output, expected_output);
}

TEST(EbmlChaptersConverter, FromXml) {
  std::string const &valid                        = "tests/unit/data/text/chapters-valid.xml";
  std::string const &with_void                    = "tests/unit/data/text/chapters-with-ebmlvoid.xml";
  std::string const &invalid_child_node           = "tests/unit/data/text/chapters-invalid-child-node.xml";
  std::string const &invalid_attribute            = "tests/unit/data/text/chapters-invalid-attribute.xml";
  std::string const &invalid_conversion           = "tests/unit/data/text/chapters-invalid-atom-missing-start.xml";
  std::string const &invalid_duplicate_child_node = "tests/unit/data/text/chapters-invalid-atom-start-twice.xml";
  std::string const &invalid_malformed_data       = "tests/unit/data/text/chapters-invalid-malformed-data.xml";
  std::string const &invalid_malformed_xml        = "tests/unit/data/text/chapters-invalid-malformed-xml.xml";
  std::string const &invalid_range                = "tests/unit/data/text/chapters-invalid-range.xml";

  EXPECT_NO_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(valid, false));
  EXPECT_NO_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(valid, true));

  EXPECT_NO_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(with_void, false));
  EXPECT_NO_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(with_void, true));

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file("does-not-exist/nonono.xml", false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file("does-not-exist/nonono.xml", true),  mtx::mm_io::open_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_child_node, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_child_node, true),  mtx::xml::invalid_child_node_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_attribute, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_attribute, true),  mtx::xml::invalid_attribute_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_conversion, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_conversion, true),  mtx::xml::conversion_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_duplicate_child_node, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_duplicate_child_node, true),  mtx::xml::duplicate_child_node_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_malformed_data, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_malformed_data, true),  mtx::xml::malformed_data_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_malformed_xml, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_malformed_xml, true),  mtx::xml::xml_parser_x);

  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_range, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_chapters_converter_c::parse_file(invalid_range, true),  mtx::xml::out_of_range_x);
}

}

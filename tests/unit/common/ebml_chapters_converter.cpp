#include "common/common_pch.h"

#include <ebml/EbmlVoid.h>

#include "gtest/gtest.h"
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

}

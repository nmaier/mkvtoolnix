#include "common/common_pch.h"

#include <ebml/EbmlVoid.h>

#include "gtest/gtest.h"
#include "tests/unit/init.h"
#include "tests/unit/util.h"

#include "common/mm_io_x.h"
#include "common/xml/ebml_segmentinfo_converter.h"

namespace {

TEST(EbmlSegmentinfoConverter, FromXml) {
  std::string const &valid              = "tests/unit/data/text/segmentinfo-valid.xml";
  std::string const &invalid_child_node = "tests/unit/data/text/segmentinfo-invalid-child-node.xml";

  EXPECT_NO_THROW(mtx::xml::ebml_segmentinfo_converter_c::parse_file(valid, false));
  EXPECT_NO_THROW(mtx::xml::ebml_segmentinfo_converter_c::parse_file(valid, true));

  EXPECT_THROW(mtx::xml::ebml_segmentinfo_converter_c::parse_file("does-not-exist/nonono.xml", false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_segmentinfo_converter_c::parse_file("does-not-exist/nonono.xml", true),  mtx::mm_io::open_x);

  EXPECT_THROW(mtx::xml::ebml_segmentinfo_converter_c::parse_file(invalid_child_node, false), mtxut::mxerror_x);
  EXPECT_THROW(mtx::xml::ebml_segmentinfo_converter_c::parse_file(invalid_child_node, true),  mtx::xml::invalid_child_node_x);
}

}

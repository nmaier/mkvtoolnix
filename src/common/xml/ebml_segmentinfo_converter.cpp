/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for segmentinfo

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/mm_io_x.h"
#include "common/strings/formatting.h"
#include "common/xml/ebml_segmentinfo_converter.h"

namespace mtx { namespace xml {

ebml_segmentinfo_converter_c::ebml_segmentinfo_converter_c()
{
  setup_maps();
}

ebml_segmentinfo_converter_c::~ebml_segmentinfo_converter_c() {
}

void
ebml_segmentinfo_converter_c::setup_maps() {
  m_debug_to_tag_name_map["NextUID"]      = "NextSegmentUID";
  m_debug_to_tag_name_map["PrevUID"]      = "PreviousSegmentUID";
  m_debug_to_tag_name_map["NextFilename"] = "NextSegmentFilename";
  m_debug_to_tag_name_map["PrevFilename"] = "PreviousSegmentFilename";

  m_invalid_elements_map["TimecodeScale"] = true;
  m_invalid_elements_map["DateUTC"]       = true;
  m_invalid_elements_map["MuxingApp"]     = true;
  m_invalid_elements_map["WritingApp"]    = true;
  m_invalid_elements_map["Duration"]      = true;
  m_invalid_elements_map["Title"]         = true;

  m_limits["SegmentUID"]                  = limits_t{ true, true, 16, 16 };
  m_limits["SegmentFamily"]               = limits_t{ true, true, 16, 16 };
  m_limits["NextSegmentUID"]              = limits_t{ true, true, 16, 16 };
  m_limits["PreviousSegmentUID"]          = limits_t{ true, true, 16, 16 };

  reverse_debug_to_tag_name_map();

  if (debugging_requested("ebml_converter_semantics"))
    dump_semantics("Info");
}

void
ebml_segmentinfo_converter_c::write_xml(KaxInfo &segmentinfo,
                                        mm_io_c &out) {
  document_cptr doc(new pugi::xml_document);

  doc->append_child(pugi::node_comment).set_value(" <!DOCTYPE Info SYSTEM \"matroskasegmentinfo.dtd\"> ");

  ebml_segmentinfo_converter_c converter;
  converter.to_xml(segmentinfo, doc);

  std::stringstream out_stream;
  doc->save(out_stream, "  ", pugi::format_default | pugi::format_write_bom);
  out.puts(out_stream.str());
}

kax_info_cptr
ebml_segmentinfo_converter_c::parse_file(std::string const &file_name,
                                         bool throw_on_error) {
  auto parse = [&]() -> std::shared_ptr<KaxInfo> {
    auto master = ebml_segmentinfo_converter_c{}.to_ebml(file_name, "Info");
    fix_mandatory_segmentinfo_elements(master.get());
    return std::dynamic_pointer_cast<KaxInfo>(master);
  };

  if (throw_on_error)
    return parse();

  try {
    return parse();

  } catch (mtx::mm_io::exception &ex) {
    mxerror(boost::format(Y("The XML segmentinfo file '%1%' could not be read.\n")) % file_name);

  } catch (mtx::xml::xml_parser_x &ex) {
    mxerror(boost::format(Y("The XML segmentinfo file '%1%' contains an error at position %3%: %2%\n")) % file_name % ex.result().description() % ex.result().offset);

  } catch (mtx::xml::exception &ex) {
    mxerror(boost::format(Y("The XML segmentinfo file '%1%' contains an error: %2%\n")) % file_name % ex.what());
  }

  return kax_info_cptr{};
}

}}

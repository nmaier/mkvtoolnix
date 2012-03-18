/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for chapters

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/strings/formatting.h"
#include "common/xml/ebml_chapters_converter.h"

namespace mtx { namespace xml {

ebml_chapters_converter_c::ebml_chapters_converter_c()
  : ebml_converter_c()
{
  setup_maps();
}

ebml_chapters_converter_c::~ebml_chapters_converter_c() {
}

void
ebml_chapters_converter_c::setup_maps() {
  m_formatter_map["ChapterTimeStart"] = format_timecode;
  m_formatter_map["ChapterTimeEnd"]   = format_timecode;
}

void
ebml_chapters_converter_c::fix_xml(xml_document_cptr &doc)
  const {
  auto result = doc->select_nodes("//ChapterAtom[not(ChapterTimeStart)]");
  for (auto &atom : result)
    atom.node().append_child("ChapterTimeStart").append_child(pugi::node_pcdata).set_value(::format_timecode(0).c_str());

  result = doc->select_nodes("//ChapterDisplay[not(ChapterString)]");
  for (auto &atom : result)
    atom.node().append_child("ChapterString");

  result = doc->select_nodes("//ChapterDisplay[not(ChapterLanguage)]");
  for (auto &atom : result)
    atom.node().append_child("ChapterLanguage").append_child(pugi::node_pcdata).set_value("eng");
}

void
ebml_chapters_converter_c::write_xml(KaxChapters &chapters,
                                     mm_io_c &out) {
  xml_document_cptr doc(new pugi::xml_document);

  doc->append_child(pugi::node_comment).set_value(" <!DOCTYPE Chapters SYSTEM \"matroskachapters.dtd\"> ");

  ebml_chapters_converter_c converter;
  converter.to_xml(chapters, doc);

  std::stringstream out_stream;
  doc->save(out_stream, "  ", pugi::format_default | pugi::format_write_bom);
  out.puts(out_stream.str());
}

}}

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

#include "common/strings/formatting.h"
#include "common/xml/ebml_chapters_xml_converter.h"

ebml_chapters_xml_converter_c::ebml_chapters_xml_converter_c()
  : ebml_xml_converter_c()
{
  setup_maps();
}

ebml_chapters_xml_converter_c::~ebml_chapters_xml_converter_c() {
}

void
ebml_chapters_xml_converter_c::setup_maps() {
  m_formatter_map["ChapterTimeStart"] = ebml_xml_converter_c::format_timecode;
  m_formatter_map["ChapterTimeEnd"]   = ebml_xml_converter_c::format_timecode;
}

void
ebml_chapters_xml_converter_c::fix_xml(xml_document_cptr &doc)
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

/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   EBML/XML converter specialization for tags

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/strings/formatting.h"
#include "common/xml/ebml_tags_xml_converter.h"

ebml_tags_xml_converter_c::ebml_tags_xml_converter_c()
  : ebml_xml_converter_c()
{
  setup_maps();
}

ebml_tags_xml_converter_c::~ebml_tags_xml_converter_c() {
}

void
ebml_tags_xml_converter_c::setup_maps() {
  m_name_map["TagTargets"]         = "Targets";
  m_name_map["TagTrackUID"]        = "TrackUID";
  m_name_map["TagEditionUID"]      = "EditionUID";
  m_name_map["TagChapterUID"]      = "ChapterUID";
  m_name_map["TagAttachmentUID"]   = "AttachmentUID";
  m_name_map["TagTargetType"]      = "TargetType";
  m_name_map["TagTargetTypeValue"] = "TargetTypeValue";
  m_name_map["TagSimple"]          = "Simple";
  m_name_map["TagName"]            = "Name";
  m_name_map["TagString"]          = "String";
  m_name_map["TagBinary"]          = "Binary";
  m_name_map["TagLanguage"]        = "TagLanguage";
  m_name_map["TagDefault"]         = "DefaultLanguage";
}

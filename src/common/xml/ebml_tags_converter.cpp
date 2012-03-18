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
#include "common/xml/ebml_tags_converter.h"

namespace mtx { namespace xml {

ebml_tags_converter_c::ebml_tags_converter_c()
  : ebml_converter_c()
{
  setup_maps();
}

ebml_tags_converter_c::~ebml_tags_converter_c() {
}

void
ebml_tags_converter_c::setup_maps() {
  m_debug_to_tag_name_map["TagTargets"]         = "Targets";
  m_debug_to_tag_name_map["TagTrackUID"]        = "TrackUID";
  m_debug_to_tag_name_map["TagEditionUID"]      = "EditionUID";
  m_debug_to_tag_name_map["TagChapterUID"]      = "ChapterUID";
  m_debug_to_tag_name_map["TagAttachmentUID"]   = "AttachmentUID";
  m_debug_to_tag_name_map["TagTargetType"]      = "TargetType";
  m_debug_to_tag_name_map["TagTargetTypeValue"] = "TargetTypeValue";
  m_debug_to_tag_name_map["TagSimple"]          = "Simple";
  m_debug_to_tag_name_map["TagName"]            = "Name";
  m_debug_to_tag_name_map["TagString"]          = "String";
  m_debug_to_tag_name_map["TagBinary"]          = "Binary";
  m_debug_to_tag_name_map["TagLanguage"]        = "TagLanguage";
  m_debug_to_tag_name_map["TagDefault"]         = "DefaultLanguage";

  reverse_debug_to_tag_name_map();
}

}}

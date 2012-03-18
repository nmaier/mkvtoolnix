/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for tags

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_XML_EBML_TAGS_XML_CONVERTER_H
#define MTX_COMMON_XML_EBML_TAGS_XML_CONVERTER_H

#include "common/common_pch.h"

#include <matroska/KaxTags.h>

#include "common/xml/ebml_converter.h"

using namespace libmatroska;

namespace mtx { namespace xml {

class ebml_tags_converter_c: public ebml_converter_c {
public:
  ebml_tags_converter_c();
  virtual ~ebml_tags_converter_c();

protected:
  void setup_maps();

public:
  static void write_xml(KaxTags &tags, mm_io_c &out);
};

}}

#endif // MTX_COMMON_XML_EBML_TAGS_XML_CONVERTER_H

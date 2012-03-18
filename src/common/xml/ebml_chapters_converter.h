/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for chapters

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_XML_EBML_CHAPTERS_XML_CONVERTER_H
#define MTX_COMMON_XML_EBML_CHAPTERS_XML_CONVERTER_H

#include "common/common_pch.h"

#include "common/xml/ebml_converter.h"

namespace mtx { namespace xml {

class ebml_chapters_converter_c: public ebml_converter_c {
public:
  ebml_chapters_converter_c();
  virtual ~ebml_chapters_converter_c();

protected:
  virtual void fix_xml(xml_document_cptr &doc) const;
  void setup_maps();
};

}}

#endif // MTX_COMMON_XML_EBML_CHAPTERS_XML_CONVERTER_H

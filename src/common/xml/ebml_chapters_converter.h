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

#include <matroska/KaxChapters.h>

#include "common/xml/ebml_converter.h"

using namespace libmatroska;

namespace mtx { namespace xml {

class ebml_chapters_converter_c: public ebml_converter_c {
public:
  ebml_chapters_converter_c();
  virtual ~ebml_chapters_converter_c();

protected:
  virtual void fix_xml(document_cptr &doc) const;
  void setup_maps();

public:
  static void write_xml(KaxChapters &chapters, mm_io_c &out);
};

}}

#endif // MTX_COMMON_XML_EBML_CHAPTERS_XML_CONVERTER_H

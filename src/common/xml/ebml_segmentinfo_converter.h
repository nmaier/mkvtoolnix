/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  EBML/XML converter specialization for segmentinfo

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_XML_EBML_SEGMENTINFO_XML_CONVERTER_H
#define MTX_COMMON_XML_EBML_SEGMENTINFO_XML_CONVERTER_H

#include "common/common_pch.h"

#include "common/segmentinfo.h"
#include "common/xml/ebml_converter.h"

using namespace libmatroska;

namespace mtx { namespace xml {

class ebml_segmentinfo_converter_c: public ebml_converter_c {
public:
  ebml_segmentinfo_converter_c();
  virtual ~ebml_segmentinfo_converter_c();

protected:
  void setup_maps();

public:
  static void write_xml(KaxInfo &segmentinfo, mm_io_c &out);
  static kax_info_cptr parse_file(std::string const &file_name, bool throw_on_error = true);
};

}}

#endif // MTX_COMMON_XML_EBML_SEGMENTINFO_XML_CONVERTER_H

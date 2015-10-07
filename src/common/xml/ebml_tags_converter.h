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

#include "common/tags/tags.h"
#include "common/xml/ebml_converter.h"

using namespace libmatroska;

namespace mtx { namespace xml {

class ebml_tags_converter_c: public ebml_converter_c {
public:
  ebml_tags_converter_c();
  virtual ~ebml_tags_converter_c();

protected:
  virtual void fix_ebml(EbmlMaster &root) const;
  virtual void fix_tag(KaxTag &tag) const;

  void setup_maps();

public:
  static void write_xml(KaxTags &tags, mm_io_c &out);
  static kax_tags_cptr parse_file(std::string const &file_name, bool throw_on_error);
};

}}

#endif // MTX_COMMON_XML_EBML_TAGS_XML_CONVERTER_H

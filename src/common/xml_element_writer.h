/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   XML chapter writer functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XML_ELEMENT_WRITER_H
#define __XML_ELEMENT_WRITER_H

#include "xml_element_parser.h"

namespace libebml {
  class EbmlElement;
};

using namespace libebml;

class mm_io_c;

struct xml_writer_cb_t {
  int level;
  int parent_idx;
  int elt_idx;
  EbmlElement *e;
  mm_io_c *out;
};

void MTX_DLL_API
write_xml_element_rec(int level, int parent_idx,
                      EbmlElement *e, mm_io_c *out,
                      const parser_element_t *element_map);

#endif // __XML_ELEMENT_WRITER_H

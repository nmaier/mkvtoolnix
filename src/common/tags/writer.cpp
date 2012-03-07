/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   writes tags in XML format

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxTags.h>

#include "common/xml/element_writer.h"

using namespace libmatroska;

void
write_tags_xml(KaxTags &tags,
               mm_io_c *out) {
  size_t i;

  for (i = 0; nullptr != tag_elements[i].name; i++) {
    tag_elements[i].start_hook = nullptr;
    tag_elements[i].end_hook   = nullptr;
  }

  for (i = 0; tags.ListSize() > i; i++)
    write_xml_element_rec(1, 0, tags[i], out, tag_elements);
}

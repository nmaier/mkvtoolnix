/*
   mkvextract -- extract tracks from Matroska files into other files
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   writes tags in XML format
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <matroska/KaxTags.h>

#include "xml_element_writer.h"

using namespace libmatroska;
using namespace std;

void
write_tags_xml(KaxTags &tags,
               mm_io_c *out) {
  int i;

  for (i = 0; tag_elements[i].name != NULL; i++) {
    tag_elements[i].start_hook = NULL;
    tag_elements[i].end_hook = NULL;
  }

  for (i = 0; i < tags.ListSize(); i++)
    write_xml_element_rec(1, 0, tags[i], out, tag_elements);
}

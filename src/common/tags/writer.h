/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   writes tags in XML format

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_TAGWRITER_H
#define __MTX_COMMON_TAGWRITER_H

#include "common/os.h"

#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>

using namespace libmatroska;

class mm_io_c;

void write_tags_xml(KaxTags &tags, mm_io_c *out);

#endif // __MTX_COMMON_TAGWRITER_H

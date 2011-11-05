/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   segment info parser/writer functions

   Written by Moritz Bunkus <moritz@bunkus.org> and
   Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __MTX_COMMON_SEGMENTINFO_H
#define __MTX_COMMON_SEGMENTINFO_H

#include "common/common_pch.h"

#include <matroska/KaxInfo.h>

#include "common/mm_io.h"

using namespace libebml;
using namespace libmatroska;

KaxInfo *parse_segmentinfo(const std::string &file_name, bool exception_on_error = false);

KaxInfo *parse_xml_segmentinfo(mm_text_io_c *in, bool exception_on_error = false);

void fix_mandatory_segmentinfo_elements(EbmlElement *e);

#endif // __MTX_COMMON_SEGMENTINFO_H


/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   segment info parser/writer functions

   Written by Moritz Bunkus <moritz@bunkus.org> and
   Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __INFOS_H
#define __INFOS_H

#include <stdio.h>

#include "ebml/EbmlElement.h"

#include "common.h"
#include "mm_io.h"

namespace libebml {
  class EbmlMaster;
};

namespace libmatroska {
  class KaxSegment;
};

using namespace libebml;
using namespace libmatroska;

KaxSegment *MTX_DLL_API
parse_infos(const string &file_name,
            bool exception_on_error = false);

KaxSegment *MTX_DLL_API parse_xml_segmentinfo(mm_text_io_c *in,
                                              bool exception_on_error = false);

void MTX_DLL_API fix_mandatory_segmentinfo_elements(EbmlElement *e);

#endif // __INFOS_H


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

#ifndef MTX_COMMON_SEGMENTINFO_H
#define MTX_COMMON_SEGMENTINFO_H

#include "common/common_pch.h"

#include <matroska/KaxInfo.h>

#include "common/mm_io.h"

using namespace libebml;
using namespace libmatroska;

typedef std::shared_ptr<KaxInfo> kax_info_cptr;

void fix_mandatory_segmentinfo_elements(EbmlElement *e);

#endif // MTX_COMMON_SEGMENTINFO_H

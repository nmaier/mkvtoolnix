/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   segment tracks helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_SEGMENT_TRACKS_H
#define __MTX_COMMON_SEGMENT_TRACKS_H

#include "os.h"

#include <ebml/EbmlElement.h>

using namespace libebml;
using namespace libmatroska;

void fix_mandatory_segment_tracks_elements(EbmlElement *e);

#endif // __MTX_COMMON_SEGMENT_TRACKS_H


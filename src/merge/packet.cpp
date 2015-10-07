/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   packet structure implementation

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/math.h"
#include "merge/cluster_helper.h"
#include "merge/output_control.h"
#include "merge/packet.h"

void
packet_t::normalize_timecodes() {
  // Normalize the timecodes according to the timecode scale.
  unmodified_assigned_timecode = assigned_timecode;
  unmodified_duration          = duration;
  timecode                     = RND_TIMECODE_SCALE(timecode);
  assigned_timecode            = RND_TIMECODE_SCALE(assigned_timecode);
  if (has_duration())
    duration                   = RND_TIMECODE_SCALE(duration);
  if (has_bref())
    bref                       = RND_TIMECODE_SCALE(bref);
  if (has_fref())
    fref                       = RND_TIMECODE_SCALE(fref);
}

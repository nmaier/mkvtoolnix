/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mkvmerge.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: mkvmerge.h,v 1.12 2003/06/07 21:59:24 mosu Exp $
    \brief definition of global variables found in mkvmerge.cpp
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include "pr_generic.h"

#include "KaxCues.h"
#include "KaxSeekHead.h"
#include "KaxSegment.h"
#include "KaxTracks.h"

using namespace LIBMATROSKA_NAMESPACE;

extern KaxSegment *kax_segment;
extern KaxTracks *kax_tracks;
extern KaxTrackEntry *kax_last_entry;
extern KaxCues *kax_cues;
extern KaxSeekHead *kax_seekhead;
extern int track_number;

extern float video_fps;

extern bool write_cues, cue_writing_requested, video_track_present;
extern bool no_lacing;

void add_packetizer(generic_packetizer_c *packetizer);
void create_next_output_file();

extern int pass;
extern int default_tracks[3];

#endif // __MKVMERGE_H

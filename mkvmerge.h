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
    \version \$Id: mkvmerge.h,v 1.16 2003/06/08 18:59:43 mosu Exp $
    \brief definition of global variables found in mkvmerge.cpp
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include <string>

#include "pr_generic.h"

#include "KaxCues.h"
#include "KaxSeekHead.h"
#include "KaxSegment.h"
#include "KaxTracks.h"

using namespace std;
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

void create_next_output_file(bool last_file = false, bool first_file = false);
void finish_file();
string create_output_name();

extern int pass, file_num, max_ms_per_cluster, max_blocks_per_cluster;
extern int default_tracks[3];
extern int64_t split_after;
extern int split_max_num_files;
extern bool split_by_time;

#endif // __MKVMERGE_H

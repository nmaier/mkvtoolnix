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
    \version $Id$
    \brief definition of global variables found in mkvmerge.cpp
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include "os.h"

#include <string>

#include "mm_io.h"
#include "pr_generic.h"

#include <matroska/KaxChapters.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTag.h>

using namespace std;
using namespace libmatroska;

extern KaxSegment *kax_segment;
extern KaxTracks *kax_tracks;
extern KaxTrackEntry *kax_last_entry;
extern KaxCues *kax_cues;
extern KaxSeekHead *kax_sh_main, *kax_sh_cues;
extern KaxChapters *kax_chapters;
extern int track_number;
extern int64_t tags_size;
extern string segment_title;
extern bool segment_title_set;

extern float video_fps;

extern bool write_cues, cue_writing_requested, video_track_present;
extern bool no_lacing, no_linking, use_timeslices, use_durations;

extern bool identifying, identify_verbose;

extern char *dump_packets;

bool hack_engaged(const char *hack);

void add_packetizer(generic_packetizer_c *packetizer);
void add_tags(KaxTag *tags);

void create_next_output_file();
void finish_file(bool last_file = false);
void rerender_track_headers();
string create_output_name();

extern int file_num;
extern bool splitting;

extern int64_t max_ns_per_cluster;
extern int max_blocks_per_cluster;
extern int default_tracks[3], default_tracks_priority[3];
extern int64_t split_after;
extern int split_max_num_files;
extern bool split_by_time;

#endif // __MKVMERGE_H

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
    \version \$Id: mkvmerge.h,v 1.5 2003/04/18 13:51:32 mosu Exp $
    \brief definition of global variables found in mkvmerge.cpp
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __MKVMERGE_H
#define __MKVMERGE_H

#include "KaxCues.h"
#include "KaxSegment.h"
#include "KaxTracks.h"

using namespace LIBMATROSKA_NAMESPACE;

extern KaxSegment *kax_segment;
extern KaxTracks *kax_tracks;
extern KaxTrackEntry *kax_last_entry;
extern KaxCues *kax_cues;
extern int track_number;

extern float video_fps;

extern int write_cues;

#endif // __MKVMERGE_H

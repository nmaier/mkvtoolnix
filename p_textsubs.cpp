/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_srt.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_textsubs.cpp,v 1.2 2003/04/11 11:23:40 mosu Exp $
    \brief Subripper subtitle reader
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "pr_generic.h"
#include "p_textsubs.h"

#include "KaxTracks.h"
#include "KaxTrackVideo.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

textsubs_packetizer_c::textsubs_packetizer_c(track_info_t *nti)
  throw (error_c): q_c(nti) {
  packetno = 0;
  set_header();
}

textsubs_packetizer_c::~textsubs_packetizer_c() {
}

#define STEXTSIMPLE "S_TEXT/SIMPLE"

void textsubs_packetizer_c::set_header() {
  using namespace LIBMATROSKA_NAMESPACE;
  
  if (kax_last_entry == NULL)
    track_entry =
      &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
  else
    track_entry =
      &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
        static_cast<KaxTrackEntry &>(*kax_last_entry));
  kax_last_entry = track_entry;
  
  if (serialno == -1)
    serialno = track_number++;
  KaxTrackNumber &tnumber =
    GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
  *(static_cast<EbmlUInteger *>(&tnumber)) = serialno;
  
  *(static_cast<EbmlUInteger *>
    (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
      track_subtitle;

  KaxCodecID &codec_id =
    GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
  codec_id.CopyBuffer((binary *)STEXTSIMPLE, countof(STEXTSIMPLE));
}

int textsubs_packetizer_c::process(unsigned char *_subs, int, int64_t start,
                                   int64_t length) {
  int num_newlines;
  char *subs, *idx1, *idx2, *tempbuf;
  int64_t end, duration, dlen, tmp;

  end = start + length;
  // Adjust the start and end values according to the audio adjustment.
  start += ti->async.displacement;
  start = (int64_t)(ti->async.linear * start);
  end += ti->async.displacement;
  end = (int64_t)(ti->async.linear * end);

  if (end < 0)
    return EMOREDATA;
  else if (start < 0)
    start = 0;

  duration = end - start;
  if (duration < 0) {
    fprintf(stderr, "Warning: textsubs_packetizer: Ignoring an entry which "
            "starts after it ends.\n");
    return EMOREDATA;
  }

  tmp = duration;
  dlen = 1;
  while (tmp >= 10) {
    tmp /= 10;
    dlen++;
  }

  idx1 = (char *)_subs;
  subs = NULL;
  num_newlines = 0;
  while (*idx1 != 0) {
    if (*idx1 == '\n')
      num_newlines++;
    idx1++;
  }
  subs = (char *)malloc(strlen((char *)_subs) + num_newlines * 2 + 1);
  if (subs == NULL)
    die("malloc");

  idx1 = (char *)_subs;
  idx2 = subs;
  while (*idx1 != 0) {
    if (*idx1 == '\n') {
      *idx2 = '\r';
      idx2++;
      *idx2 = '\n';
      idx2++;
    } else if (*idx1 != '\r') {
      *idx2 = *idx1;
      idx2++;
    }
    idx1++;
  }
  if (idx2 != subs) {
    while (((idx2 - 1) != subs) &&
           ((*(idx2 - 1) == '\n') || (*(idx2 - 1) == '\r'))) {
      *idx2 = 0;
      idx2--;
    }
  }
  *idx2 = 0;

  tempbuf = (char *)malloc(strlen(subs) + dlen + 2 + 1);
  if (tempbuf == NULL)
    die("malloc");
  sprintf(tempbuf, "%lld\r\n", duration);
  strcat(tempbuf, subs);

  add_packet((unsigned char *)tempbuf, strlen(tempbuf), start);

  free(tempbuf);
  free(subs);

  return EMOREDATA;
}

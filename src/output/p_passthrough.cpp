/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_passthrough.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief PASSTHROUGH output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "p_passthrough.h"
#include "matroska.h"

using namespace libmatroska;

passthrough_packetizer_c::passthrough_packetizer_c(generic_reader_c *nreader,
                                                   track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  packets_processed = 0;
  bytes_processed = 0;
  duration_forced = 0;
}

void
passthrough_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

void
passthrough_packetizer_c::force_duration(int n_frames) {
  duration_forced += n_frames;
}

int
passthrough_packetizer_c::process(unsigned char *buf,
                                  int size,
                                  int64_t timecode,
                                  int64_t duration,
                                  int64_t bref,
                                  int64_t fref) {
  bool forced;

  debug_enter("passthrough_packetizer_c::process");

  if (duration_forced > 0) {
    forced = true;
    duration_forced--;
  } else
    forced = false;
  packets_processed++;
  bytes_processed += size;
  if (needs_negative_displacement(duration)) {
    displace(-duration);
    sync_to_keyframe = true;
    return EMOREDATA;
  }
  while (needs_positive_displacement(duration)) {
    add_packet(buf, size,
               (int64_t)((timecode + ti->async.displacement) *
                         ti->async.linear),
               duration, forced, bref, fref);
    displace(duration);
  }

  if (sync_to_keyframe && (bref != -1)) {
    if (!duplicate_data)
      safefree(buf);
    return EMOREDATA;
  }
  sync_to_keyframe = false;
  timecode = (int64_t)((timecode + ti->async.displacement) * ti->async.linear);
  duration = (int64_t)(duration * ti->async.linear);
  add_packet(buf, size, timecode, duration, forced, bref, fref);

  debug_leave("passthrough_packetizer_c::process");
  return EMOREDATA;
}

void
passthrough_packetizer_c::dump_debug_info() {
  mxdebug("passthrough_packetizer_c: packets processed: %lld, "
          "bytes processed: %lld, packets in queue: %d\n",
          packets_processed, bytes_processed, packet_queue.size());
}

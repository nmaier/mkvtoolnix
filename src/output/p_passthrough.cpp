/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * PASSTHROUGH output module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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
  sync_to_keyframe = false;
}

void
passthrough_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

int
passthrough_packetizer_c::process(memory_c &mem,
                                  int64_t timecode,
                                  int64_t duration,
                                  int64_t bref,
                                  int64_t fref) {
  return process(mem, timecode, duration, bref, fref, false);
}

int
passthrough_packetizer_c::process(memory_c &mem,
                                  int64_t timecode,
                                  int64_t duration,
                                  int64_t bref,
                                  int64_t fref,
                                  bool duration_mandatory) {
  debug_enter("passthrough_packetizer_c::process");

  packets_processed++;
  bytes_processed += mem.size;
  if ((duration > 0) && needs_negative_displacement(duration)) {
    displace(-duration);
    sync_to_keyframe = true;
    return EMOREDATA;
  }
  if ((duration > 0) && needs_positive_displacement(duration) &&
      sync_complete_group) {
    // Not implemented yet.
  }
  while ((duration > 0) && needs_positive_displacement(duration)) {
    add_packet(mem,
               (int64_t)((timecode + ti->async.displacement) *
                         ti->async.linear),
               duration, duration_mandatory, bref, fref);
    displace(duration);
  }

  if (sync_to_keyframe && (bref != -1))
    return EMOREDATA;
  sync_to_keyframe = false;
  timecode = (int64_t)((timecode + ti->async.displacement) * ti->async.linear);
  duration = (int64_t)(duration * ti->async.linear);
  if (bref >= 0)
    bref = (int64_t)((bref + ti->async.displacement) * ti->async.linear);
  if (fref >= 0)
    fref = (int64_t)((fref + ti->async.displacement) * ti->async.linear);
  add_packet(mem, timecode, duration, duration_mandatory, bref, fref);

  debug_leave("passthrough_packetizer_c::process");
  return EMOREDATA;
}

void
passthrough_packetizer_c::always_sync_complete_group(bool sync) {
  sync_complete_group = sync;
}

void
passthrough_packetizer_c::dump_debug_info() {
  mxdebug("passthrough_packetizer_c: packets processed: %lld, "
          "bytes processed: %lld, packets in queue: %d\n",
          packets_processed, bytes_processed, packet_queue.size());
}

int
passthrough_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  passthrough_packetizer_c *psrc;

  psrc = dynamic_cast<passthrough_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  // For now...
  return CAN_CONNECT_NO_PARAMETERS;
}

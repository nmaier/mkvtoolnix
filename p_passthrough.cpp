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

using namespace LIBMATROSKA_NAMESPACE;

passthrough_packetizer_c::passthrough_packetizer_c(generic_reader_c *nreader,
                                                   track_info_t *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  packets_processed = 0;
  bytes_processed = 0;
}

void passthrough_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

int passthrough_packetizer_c::process(unsigned char *buf, int size,
                                      int64_t timecode, int64_t duration,
                                      int64_t bref, int64_t fref) {
  debug_enter("passthrough_packetizer_c::process");

  packets_processed++;
  bytes_processed += size;

  add_packet(buf, size, timecode, duration, false, bref, fref);

  debug_leave("passthrough_packetizer_c::process");
  return EMOREDATA;
}

void passthrough_packetizer_c::dump_debug_info() {
  fprintf(stderr, "DBG> passthrough_packetizer_c: packets processed: %lld, "
          "bytes processed: %lld, packets in queue: %d\n",
          packets_processed, bytes_processed, packet_queue.size());
}

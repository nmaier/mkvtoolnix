/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_flac.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief FLAC packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "config.h"

#if defined(HAVE_FLAC_FORMAT_H)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(COMP_MSC)
#include <unistd.h>
#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "pr_generic.h"
#include "p_flac.h"
#include "matroska.h"
#include "mkvmerge.h"

using namespace libmatroska;

flac_packetizer_c::flac_packetizer_c(generic_reader_c *nreader,
                                     int nsample_rate, int nchannels,
                                     int nbits_per_sample,
                                     unsigned char *nheader, int nl_header,
                                     track_info_c *nti)
  throw (error_c): generic_packetizer_c(nreader, nti) {
  last_timecode = 0;
  sample_rate = nsample_rate;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  if ((nl_header < 4) || (nheader[0] != 'f') || (nheader [1] != 'L') ||
      (nheader[2] != 'a') || (nheader[3] != 'C')) {
    header = (unsigned char *)safemalloc(nl_header + 4);
    memcpy(header, "fLaC", 4);
    memcpy(&header[4], nheader, nl_header);
    l_header = nl_header + 4;
  } else {
    header = (unsigned char *)safememdup(nheader, nl_header);
    l_header = nl_header;
  }

  set_track_type(track_audio);
  if (use_durations)
    die("flac_packetizer: use_durations() not supported yet.\n");
  if ((ti->async.displacement != 0) || (ti->async.linear != 1.0))
    mxwarn("FLAC packetizer: Audio synchronization has not been implemented "
           "yet for FLAC.\n");
}

flac_packetizer_c::~flac_packetizer_c() {
  safefree(header);
}

void flac_packetizer_c::set_headers() {
  set_codec_id(MKV_A_FLAC);
  set_codec_private(header, l_header);
  set_audio_sampling_freq((float)sample_rate);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);

  generic_packetizer_c::set_headers();
}

int flac_packetizer_c::process(unsigned char *data, int size,
                                 int64_t timecode, int64_t, int64_t, int64_t) {
  debug_enter("flac_packetizer_c::process");
  if (timecode == -1)
    timecode = last_timecode;
  last_timecode = timecode;
  add_packet(data, size, timecode, 0);
  debug_leave("flac_packetizer_c::process");

  return EMOREDATA;
}

void flac_packetizer_c::dump_debug_info() {
  mxdebug("flac_packetizer_c: queue: %d\n", packet_queue.size());
}
#endif

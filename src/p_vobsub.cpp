/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vobsub.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief VobSub packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lzo1x.h>

#include "common.h"
#include "p_vobsub.h"
#include "matroska.h"

using namespace libmatroska;

vobsub_packetizer_c::vobsub_packetizer_c(generic_reader_c *nreader,
                                         const void *nidx_data,
                                         int nidx_data_size,
                                         const void *nifo_data,
                                         int nifo_data_size,
                                         int ncompression_type,
                                         int ncompressed_type,
                                         track_info_t *nti) throw (error_c):
  generic_packetizer_c(nreader, nti) {

  idx_data = (unsigned char *)safememdup(nidx_data, nidx_data_size);
  idx_data_size = nidx_data_size;

  if ((nifo_data != NULL) && (nifo_data_size != 0)) {
    ifo_data = (unsigned char *)safememdup(nifo_data, nifo_data_size);
    ifo_data_size = nifo_data_size;
  } else {
    ifo_data = NULL;
    ifo_data_size = 0;
  }

  compression_type = ncompression_type;
  compressed_type = ncompressed_type;

  set_track_type(track_subtitle);
}

void vobsub_packetizer_c::set_headers() {
  string codec_id;
  unsigned char *priv;
  int priv_size, i, size_tmp;

  codec_id = MKV_S_VOBSUB;
  if (compression_type == COMPRESSION_LZO)
    codec_id += "/LZO";
  else if (compression_type == COMPRESSION_ZLIB)
    codec_id += "/Z";
  else if (compression_type == COMPRESSION_BZ2)
    codec_id += "/BZ2";
  set_codec_id(codec_id.c_str());

  priv_size = idx_data_size + 1;
  if (ifo_data_size > 0)
    priv_size += ifo_data_size + idx_data_size / 255 + 1;

  priv = (unsigned char *)safemalloc(priv_size);
  i = 1;
  if (ifo_data_size > 0) {
    priv[0] = 1;
    size_tmp = ifo_data_size;
    while (size_tmp >= 255) {
      priv[i] = 0xff;
      i++;
      size_tmp -= 255;
    }
    priv[i] = size_tmp;
    i++;
    memcpy(&priv[i + idx_data_size], ifo_data, ifo_data_size);
  } else {
    priv[0] = 0;
  }
  memcpy(&priv[i], idx_data, idx_data_size);
  set_codec_private(priv, priv_size);
  safefree(priv);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int vobsub_packetizer_c::process(unsigned char *buf, int size,
                                int64_t timecode, int64_t duration,
                                int64_t, int64_t) {
  debug_enter("vobsub_packetizer_c::process");

  add_packet(buf, size, timecode, duration, true);

  debug_leave("vobsub_packetizer_c::process");

  return EMOREDATA;
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
  safefree(idx_data);
  safefree(ifo_data);
}

void vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}

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

#include "common.h"
#include "compression.h"
#include "p_vobsub.h"
#include "matroska.h"

using namespace libmatroska;

vobsub_packetizer_c::vobsub_packetizer_c(generic_reader_c *nreader,
                                         const void *nidx_data,
                                         int nidx_data_size,
                                         int ncompression_type,
                                         int ncompressed_type,
                                         track_info_t *nti) throw (error_c):
  generic_packetizer_c(nreader, nti) {
  const char *compression;

  idx_data = (unsigned char *)safememdup(nidx_data, nidx_data_size);
  idx_data_size = nidx_data_size;

  if (ti->compression == COMPRESSION_UNSPECIFIED)
    compression_type = ncompression_type;
  else
    compression_type = ti->compression;
  compressed_type = ncompressed_type;

  raw_size = 0;
  compressed_size = 0;
  items = 0;

  set_track_type(track_subtitle);

  compressor = NULL;
  decompressor = NULL;

  compression = "no";
  if (compression_type != compressed_type) {
    compression = NULL;

    compressor = compression_c::create(compression_type);
    if (compressor == NULL)
      mxerror("vobsub_packetizer: Compression schmeme %d not implemented.",
              compression_type);

    if (compression_type != COMPRESSION_NONE)
      compression = compression_schemes[compression_type];

    decompressor = compression_c::create(compressed_type);
    if (decompressor == NULL)
      mxerror("vobsub_packetizer: Compression schmeme %d not implemented.",
              compressed_type);
  }

  mxverb(2, "vobsub_packetizer: Using %s compression.\n", compression);
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
  if (compressor != NULL)
    delete compressor;
  if (decompressor != NULL)
    delete decompressor;
  safefree(idx_data);
}

void vobsub_packetizer_c::set_headers() {
  string codec_id;

  codec_id = MKV_S_VOBSUB;
  if (compression_type == COMPRESSION_LZO)
    codec_id += "/LZO";
  else if (compression_type == COMPRESSION_ZLIB)
    codec_id += "/ZLIB";
  else if (compression_type == COMPRESSION_BZ2)
    codec_id += "/BZ2";
  set_codec_id(codec_id.c_str());

  set_codec_private(idx_data, idx_data_size);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int vobsub_packetizer_c::process(unsigned char *buf, int size,
                                int64_t timecode, int64_t duration,
                                int64_t, int64_t) {
  unsigned char *decompressed_buf, *final_buf;
  bool del_decompressed_buf, del_final_buf;
  int new_size;

  debug_enter("vobsub_packetizer_c::process");

  if (compression_type == compressed_type)
    add_packet(buf, size, timecode, duration, true);
  else {
    new_size = size;
    if (compressed_type != COMPRESSION_NONE) {
      decompressed_buf = decompressor->decompress(buf, new_size);
      del_decompressed_buf = true;
    } else {
      decompressed_buf = buf;
      del_decompressed_buf = false;
    }

    if (compression_type != COMPRESSION_NONE) {
      final_buf = compressor->compress(decompressed_buf, new_size);
      del_final_buf = true;
    } else {
      final_buf = decompressed_buf;
      del_final_buf = false;
    }

    add_packet(final_buf, new_size, timecode, duration, true);
    if (del_decompressed_buf)
      safefree(decompressed_buf);
    if (del_final_buf)
      safefree(final_buf);
  }

  debug_leave("vobsub_packetizer_c::process");

  return EMOREDATA;
}

void vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}

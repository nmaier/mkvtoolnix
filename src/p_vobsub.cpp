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
#include <lzoutil.h>
#include <zlib.h>

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
  int result;
  const char *compression;

  idx_data = (unsigned char *)safememdup(nidx_data, nidx_data_size);
  idx_data_size = nidx_data_size;

  if ((nifo_data != NULL) && (nifo_data_size != 0)) {
    ifo_data = (unsigned char *)safememdup(nifo_data, nifo_data_size);
    ifo_data_size = nifo_data_size;
  } else {
    ifo_data = NULL;
    ifo_data_size = 0;
  }

  if (nti->compression == COMPRESSION_UNSPECIFIED)
    compression_type = ncompression_type;
  else
    compression_type = nti->compression;
  compressed_type = ncompressed_type;

  raw_size = 0;
  compressed_size = 0;
  items = 0;

  set_track_type(track_subtitle);

  if (compression_type != compressed_type) {
    if (compression_type == COMPRESSION_LZO) {
      if ((result = lzo_init()) != LZO_E_OK)
        mxerror("vobsub_packetizer: lzo_init() failed. Result: %d\n", result);
      lzo_wrkmem = (lzo_bytep)lzo_malloc(LZO1X_999_MEM_COMPRESS);
      if (lzo_wrkmem == NULL)
        mxerror("vobsub_packetizer: lzo_malloc(LZO1X_999_MEM_COMPRESS) failed."
                "\n");
      compression = "LZO";

    } else if (compression_type == COMPRESSION_ZLIB) {
      zc_stream.zalloc = (alloc_func)0;
      zc_stream.zfree = (free_func)0;
      zc_stream.opaque = (voidpf)0;
      result = deflateInit(&zc_stream, 9);
      if (result != Z_OK)
        mxerror("vobsub_packetizer: deflateInit() failed. Result: %d\n",
                result);
      compression = "Zlib";

    } else if (compression_type == COMPRESSION_BZ2) {
      bzc_stream.bzalloc = NULL;
      bzc_stream.bzfree = NULL;
      bzc_stream.opaque = NULL;

      result = BZ2_bzCompressInit(&bzc_stream, 9, 0, 30);
      if (result != BZ_OK)
        mxerror("vobsub_packetizer: BZ2_bzCompressInit() failed. Result: %d\n",
                result);
      compression = "bzip2";

    } else if (compression_type != COMPRESSION_NONE)
      die("vobsub_packetizer: Compression schmeme %d not implemented.",
          compression_type);
    else
      compression = "no";

    if (compressed_type == COMPRESSION_ZLIB) {
      zd_stream.zalloc = (alloc_func)0;
      zd_stream.zfree = (free_func)0;
      zd_stream.opaque = (voidpf)0;
      result = inflateInit(&zd_stream);
      if (result != Z_OK)
        mxerror("vobsub_packetizer: inflateInit() failed. Result: %d\n",
                result);

    } else if (compressed_type != COMPRESSION_NONE)
      die("vobsub_packetizer: Compression schmeme %d not implemented.",
          compressed_type);
  }

  mxverb(2, "vobsub_packetizer: Using %s compression.\n", compression);
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
  safefree(idx_data);
  safefree(ifo_data);

  if (compression_type != compressed_type) {
    if (compression_type == COMPRESSION_LZO) {
      safefree(lzo_wrkmem);
    } else if (compression_type == COMPRESSION_ZLIB)
      deflateEnd(&zc_stream);
    else if (compression_type == COMPRESSION_BZ2)
      BZ2_bzCompressEnd(&bzc_stream);

    if (compressed_type == COMPRESSION_ZLIB)
      inflateEnd(&zd_stream);
  }

  if (items != 0)
    mxverb(2, "vobsub_packetizer: Overall stats: raw size: %lld, compressed "
           "size: %lld, items: %lld, ratio: %.2f%%, avg bytes per item: "
           "%lld\n", raw_size, compressed_size, items,
           compressed_size * 100.0 / raw_size, compressed_size / items);
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
    size_tmp = idx_data_size;
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

unsigned char *vobsub_packetizer_c::uncompress(unsigned char *buffer,
                                               int &size) {
  unsigned char *dst;
  int result, dstsize;

  dst = (unsigned char *)safemalloc(size * 20);

  if (compressed_type == COMPRESSION_ZLIB) {
    zd_stream.next_in = (Bytef *)buffer;
    zd_stream.next_out = (Bytef *)dst;
    zd_stream.avail_in = size;
    zd_stream.avail_out = 20 * size;
    result = inflate(&zd_stream, Z_FULL_FLUSH);
    if (result != Z_OK)
      mxerror("vobsub_packetizer: Zlib decompression failed. Result: %d\n",
              result);

    dstsize = 20 * size - zd_stream.avail_out;
  }

  mxverb(3, "vobsub_packetizer: Decompression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  dst = (unsigned char *)saferealloc(dst, dstsize);
  size = dstsize;

  return dst;
}

unsigned char *vobsub_packetizer_c::compress(unsigned char *buffer,
                                             int &size) {
  unsigned char *dst;
  int result, dstsize;

  dst = (unsigned char *)safemalloc(size * 2);

  if (compression_type == COMPRESSION_LZO) {
    lzo_uint lzo_dstsize = size * 2;
    if ((result = lzo1x_999_compress(buffer, size, dst, &lzo_dstsize,
                                     lzo_wrkmem)) != LZO_E_OK)
      mxerror("vobsub_packetizer: LZO compression failed. Result: %d\n",
              result);
    dstsize = lzo_dstsize;

  } else if (compression_type == COMPRESSION_ZLIB) {
    zc_stream.next_in = (Bytef *)buffer;
    zc_stream.next_out = (Bytef *)dst;
    zc_stream.avail_in = size;
    zc_stream.avail_out = 2 * size;
    result = deflate(&zc_stream, Z_FULL_FLUSH);
    if (result != Z_OK)
      mxerror("vobsub_packetizer: Zlib compression failed. Result: %d\n",
              result);

    dstsize = 2 * size - zc_stream.avail_out;

  } else if (compression_type == COMPRESSION_BZ2) {
    bzc_stream.next_in = (char *)buffer;
    bzc_stream.next_out = (char *)dst;
    bzc_stream.avail_in = size;
    bzc_stream.avail_out = 2 * size;
    result = BZ2_bzCompress(&bzc_stream, BZ_FLUSH);
    if (result != BZ_RUN_OK)
      mxerror("vobsub_packetizer: bzip2 compression failed. Result: %d\n",
              result);

    dstsize = 2 * size - bzc_stream.avail_out;

  }

  mxverb(3, "vobsub_packetizer: Compression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  raw_size += size;
  compressed_size += dstsize;
  items++;

  dst = (unsigned char *)saferealloc(dst, dstsize);
  size = dstsize;

  return dst;
}

int vobsub_packetizer_c::process(unsigned char *buf, int size,
                                int64_t timecode, int64_t duration,
                                int64_t, int64_t) {
  unsigned char *uncompressed_buf, *final_buf;
  bool del_uncompressed_buf, del_final_buf;
  int new_size;

  debug_enter("vobsub_packetizer_c::process");

  if (compression_type == compressed_type)
    add_packet(buf, size, timecode, duration, true);
  else {
    new_size = size;
    if (compressed_type != COMPRESSION_NONE) {
      uncompressed_buf = uncompress(buf, new_size);
      del_uncompressed_buf = true;
    } else {
      uncompressed_buf = buf;
      del_uncompressed_buf = false;
    }

    if (compression_type != COMPRESSION_NONE) {
      final_buf = compress(uncompressed_buf, new_size);
      del_final_buf = true;
    } else {
      final_buf = uncompressed_buf;
      del_final_buf = false;
    }

    add_packet(final_buf, new_size, timecode, duration, true);
    if (del_uncompressed_buf)
      safefree(uncompressed_buf);
    if (del_final_buf)
      safefree(final_buf);
  }

  debug_leave("vobsub_packetizer_c::process");

  return EMOREDATA;
}

void vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}

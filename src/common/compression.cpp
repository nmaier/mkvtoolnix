/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  compression.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief Helper routines for various compression libs
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "compression.h"

const char *compression_schemes[] = {
  "unspecified", "zlib", "bz2", "lzo", "none"
};

#if defined(HAVE_LZO1X_H)
#include <lzoutil.h>

lzo_compression_c::lzo_compression_c():
  compression_c(COMPRESSION_LZO) {
  int result;

  if ((result = lzo_init()) != LZO_E_OK)
    mxerror("lzo_init() failed. Result: %d\n", result);
  wrkmem = (lzo_bytep)lzo_malloc(LZO1X_999_MEM_COMPRESS);
  if (wrkmem == NULL)
    mxerror("lzo_malloc(LZO1X_999_MEM_COMPRESS) failed.\n");
}

lzo_compression_c::~lzo_compression_c() {
  safefree(wrkmem);
}

unsigned char *lzo_compression_c::decompress(unsigned char *buffer,
                                             int &size) {
  die("lzo_compression_c::decompress() not implemented\n");

  return NULL;
}

unsigned char *lzo_compression_c::compress(unsigned char *buffer, int &size) {
  unsigned char *dst;
  int result, dstsize;

  dst = (unsigned char *)safemalloc(size * 2);

  lzo_uint lzo_dstsize = size * 2;
  if ((result = lzo1x_999_compress(buffer, size, dst, &lzo_dstsize,
                                   wrkmem)) != LZO_E_OK)
    mxerror("LZO compression failed. Result: %d\n", result);
  dstsize = lzo_dstsize;

  mxverb(3, "lzo_compression_c: Compression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  raw_size += size;
  compressed_size += dstsize;
  items++;

  dst = (unsigned char *)saferealloc(dst, dstsize);
  size = dstsize;

  return dst;
}

#endif // HAVE_LZO1X_H

#if defined(HAVE_ZLIB_H)
zlib_compression_c::zlib_compression_c():
  compression_c(COMPRESSION_ZLIB) {
}

zlib_compression_c::~zlib_compression_c() {
}

unsigned char *zlib_compression_c::decompress(unsigned char *buffer,
                                             int &size) {
  unsigned char *dst;
  int result, dstsize;
  z_stream d_stream;

  dst = (unsigned char *)safemalloc(size * 20);

  d_stream.zalloc = (alloc_func)0;
  d_stream.zfree = (free_func)0;
  d_stream.opaque = (voidpf)0;
  result = inflateInit(&d_stream);
  if (result != Z_OK)
    mxerror("inflateInit() failed. Result: %d\n", result);

  d_stream.next_in = (Bytef *)buffer;
  d_stream.next_out = (Bytef *)dst;
  d_stream.avail_in = size;
  d_stream.avail_out = 20 * size;
  result = inflate(&d_stream, Z_FULL_FLUSH);
  if (result != Z_OK)
    mxerror("Zlib decompression failed. Result: %d\n", result);

  dstsize = 20 * size - d_stream.avail_out;

  mxverb(3, "zlib_compression_c: Decompression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  dst = (unsigned char *)saferealloc(dst, dstsize);
  size = dstsize;

  inflateEnd(&d_stream);

  return dst;
}

unsigned char *zlib_compression_c::compress(unsigned char *buffer, int &size) {
  unsigned char *dst;
  int result, dstsize;
  z_stream c_stream;

  dst = (unsigned char *)safemalloc(size * 2);

  c_stream.zalloc = (alloc_func)0;
  c_stream.zfree = (free_func)0;
  c_stream.opaque = (voidpf)0;
  result = deflateInit(&c_stream, 9);
  if (result != Z_OK)
    mxerror("deflateInit() failed. Result: %d\n", result);

  c_stream.next_in = (Bytef *)buffer;
  c_stream.next_out = (Bytef *)dst;
  c_stream.avail_in = size;
  c_stream.avail_out = 2 * size;
  result = deflate(&c_stream, Z_FULL_FLUSH);
  if (result != Z_OK)
    mxerror("Zlib compression failed. Result: %d\n", result);

  dstsize = 2 * size - c_stream.avail_out;

  mxverb(3, "zlib_compression_c: Compression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  raw_size += size;
  compressed_size += dstsize;
  items++;

  dst = (unsigned char *)saferealloc(dst, dstsize);
  size = dstsize;

  deflateEnd(&c_stream);

  return dst;
}

#endif // HAVE_ZLIB_H

#if defined(HAVE_BZLIB_H)
bzlib_compression_c::bzlib_compression_c():
  compression_c(COMPRESSION_BZ2) {
}

bzlib_compression_c::~bzlib_compression_c() {
}

unsigned char *bzlib_compression_c::decompress(unsigned char *buffer,
                                             int &size) {
  int result;
  bz_stream d_stream;

  die("bzlib_compression_c::decompress() not implemented\n");

  d_stream.bzalloc = NULL;
  d_stream.bzfree = NULL;
  d_stream.opaque = NULL;

  result = BZ2_bzDecompressInit(&d_stream, 0, 0);
  if (result != BZ_OK)
    mxerror("BZ2_bzCompressInit() failed. Result: %d\n", result);
  BZ2_bzDecompressEnd(&d_stream);

  return NULL;
}

unsigned char *bzlib_compression_c::compress(unsigned char *buffer,
                                             int &size) {
  unsigned char *dst;
  int result, dstsize;
  bz_stream c_stream;

  dst = (unsigned char *)safemalloc(size * 2);

  c_stream.bzalloc = NULL;
  c_stream.bzfree = NULL;
  c_stream.opaque = NULL;

  result = BZ2_bzCompressInit(&c_stream, 9, 0, 30);
  if (result != BZ_OK)
    mxerror("BZ2_bzCompressInit() failed. Result: %d\n", result);

  c_stream.next_in = (char *)buffer;
  c_stream.next_out = (char *)dst;
  c_stream.avail_in = size;
  c_stream.avail_out = 2 * size;
  result = BZ2_bzCompress(&c_stream, BZ_FLUSH);
  if (result != BZ_RUN_OK)
    mxerror("bzip2 compression failed. Result: %d\n", result);

  dstsize = 2 * size - c_stream.avail_out;

  mxverb(3, "bzlib_compression_c: Compression from %d to %d, %d%%\n",
         size, dstsize, dstsize * 100 / size);

  raw_size += size;
  compressed_size += dstsize;
  items++;

  dst = (unsigned char *)saferealloc(dst, dstsize);
  size = dstsize;

  BZ2_bzCompressEnd(&c_stream);

  return dst;
}

#endif // HAVE_BZLIB_H

compression_c::~compression_c() {
  if (items != 0)
    mxverb(2, "compression: Overall stats: raw size: %lld, compressed "
           "size: %lld, items: %lld, ratio: %.2f%%, avg bytes per item: "
           "%lld\n", raw_size, compressed_size, items,
           compressed_size * 100.0 / raw_size, compressed_size / items);
}

compression_c *compression_c::create(int scheme) {
  if ((scheme <= COMPRESSION_UNSPECIFIED) ||
      (scheme > COMPRESSION_NUM))
    return NULL;

  return create(compression_schemes[scheme]);
}

compression_c *compression_c::create(const char *scheme) {
#if defined(HAVE_LZO1X_H)
  if (!strcasecmp(scheme, compression_schemes[COMPRESSION_LZO]))
    return new lzo_compression_c();
#endif // HAVE_LZO1X_H

#if defined(HAVE_ZLIB_H)
  if (!strcasecmp(scheme, compression_schemes[COMPRESSION_ZLIB]))
    return new zlib_compression_c();
#endif // HAVE_ZLIB_H

#if defined(HAVE_BZLIB_H)
  if (!strcasecmp(scheme, compression_schemes[COMPRESSION_BZ2]))
    return new bzlib_compression_c();
#endif // HAVE_BZLIB_H

  if (!strcasecmp(scheme, "none"))
    return new compression_c(COMPRESSION_NONE);

  return NULL;
}

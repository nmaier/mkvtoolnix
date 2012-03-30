/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   bzlib compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(HAVE_BZLIB_H)

# include "common/compression/bzlib.h"

bzlib_compressor_c::bzlib_compressor_c()
  : compressor_c(COMPRESSION_BZ2)
{
}

bzlib_compressor_c::~bzlib_compressor_c() {
}

memory_cptr
bzlib_compressor_c::do_decompress(memory_cptr const &buffer) {
  bz_stream d_stream;

  d_stream.bzalloc = nullptr;
  d_stream.bzfree  = nullptr;
  d_stream.opaque  = nullptr;
  int result       = BZ2_bzDecompressInit(&d_stream, 0, 0);

  if (BZ_OK != result)
    mxerror(boost::format(Y("BZ2_bzCompressInit() failed. Result: %1%\n")) % result);

  auto in_size             = buffer->get_size();
  memory_cptr uncompressed = memory_c::alloc(in_size);
  d_stream.next_in         = reinterpret_cast<char *>(buffer->get_buffer());
  d_stream.avail_in        = in_size;

  while (1) {
    d_stream.next_out  = reinterpret_cast<char *>(uncompressed->get_buffer() + uncompressed->get_size() - in_size);
    d_stream.avail_out = in_size;

    do {
      result = BZ2_bzDecompress(&d_stream);
    } while ((BZ_OK == result) && (0 < d_stream.avail_out));

    if (BZ_STREAM_END == result)
      break;

    if (0 == d_stream.avail_out) {
      uncompressed->resize(uncompressed->get_size() + in_size);
      continue;
    }

    throw mtx::compression_x(boost::format(Y("BZ2_bzDecompress() failed. Result: %1%\n")) % result);
  }

  uncompressed->resize(uncompressed->get_size() - d_stream.avail_out);

  BZ2_bzDecompressEnd(&d_stream);

  return uncompressed;
}

memory_cptr
bzlib_compressor_c::do_compress(memory_cptr const &buffer) {
  bz_stream c_stream;

  int size           = buffer->get_size();
  memory_cptr dst    = memory_c::alloc(size * 2);

  c_stream.bzalloc   = nullptr;
  c_stream.bzfree    = nullptr;
  c_stream.opaque    = nullptr;

  int result         = BZ2_bzCompressInit(&c_stream, 9, 0, 30);
  if (BZ_OK != result)
    mxerror(boost::format(Y("BZ2_bzCompressInit() failed. Result: %1%\n")) % result);

  c_stream.next_in   = (char *)buffer->get_buffer();
  c_stream.next_out  = reinterpret_cast<char *>(dst->get_buffer());
  c_stream.avail_in  = size;
  c_stream.avail_out = 2 * size;
  result             = BZ2_bzCompress(&c_stream, BZ_FINISH);

  if (BZ_STREAM_END != result)
    mxerror(boost::format(Y("bzip2 compression failed. Result: %1%\n")) % result);

  BZ2_bzCompressEnd(&c_stream);

  dst->resize(dst->get_size() - c_stream.avail_out);

  mxverb(3, boost::format("bzlib_compressor_c: Compression from %1% to %2%, %3%%%\n") % size % dst->get_size() % (dst->get_size() * 100 / size));

  raw_size        += size;
  compressed_size += dst->get_size();
  items++;

  return dst;
}

#endif // HAVE_BZLIB_H

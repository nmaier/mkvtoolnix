/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   zlib compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/compression/zlib.h"

zlib_compressor_c::zlib_compressor_c()
  : compressor_c(COMPRESSION_ZLIB)
{
}

zlib_compressor_c::~zlib_compressor_c() {
}

memory_cptr
zlib_compressor_c::do_decompress(memory_cptr const &buffer) {
  z_stream d_stream;

  d_stream.zalloc = (alloc_func)0;
  d_stream.zfree  = (free_func)0;
  d_stream.opaque = (voidpf)0;
  int result      = inflateInit2(&d_stream, 15 + 32); // 15: window size; 32: look for zlib/gzip headers automatically

  if (Z_OK != result)
    mxerror(boost::format(Y("inflateInit() failed. Result: %1%\n")) % result);

  d_stream.next_in   = reinterpret_cast<Bytef *>(buffer->get_buffer());
  d_stream.avail_in  = buffer->get_size();
  int n              = 0;
  memory_cptr dst    = memory_c::alloc(0);

  do {
    n++;
    dst->resize(n * 4000);
    d_stream.next_out  = reinterpret_cast<Bytef *>(dst->get_buffer() + (n - 1) * 4000);
    d_stream.avail_out = 4000;
    result             = inflate(&d_stream, Z_NO_FLUSH);

    if ((Z_OK != result) && (Z_STREAM_END != result))
      throw mtx::compression_x(boost::format(Y("Zlib decompression failed. Result: %1%\n")) % result);

  } while ((0 == d_stream.avail_out) && (0 != d_stream.avail_in) && (Z_STREAM_END != result));

  dst->resize(d_stream.total_out);
  inflateEnd(&d_stream);

  mxverb(3, boost::format("zlib_compressor_c: Decompression from %1% to %2%, %3%%%\n") % buffer->get_size() % dst->get_size() % (dst->get_size() * 100 / buffer->get_size()));

  return dst;
}

memory_cptr
zlib_compressor_c::do_compress(memory_cptr const &buffer) {
  z_stream c_stream;

  c_stream.zalloc = (alloc_func)0;
  c_stream.zfree  = (free_func)0;
  c_stream.opaque = (voidpf)0;
  int result      = deflateInit(&c_stream, 9);

  if (Z_OK != result)
    mxerror(boost::format(Y("deflateInit() failed. Result: %1%\n")) % result);

  c_stream.next_in   = (Bytef *)buffer->get_buffer();
  c_stream.avail_in  = buffer->get_size();
  int n              = 0;
  memory_cptr dst    = memory_c::alloc(0);

  do {
    n++;
    dst->resize(n * 4000);
    c_stream.next_out  = reinterpret_cast<Bytef *>(dst->get_buffer() + (n - 1) * 4000);
    c_stream.avail_out = 4000;
    result             = deflate(&c_stream, Z_FINISH);

    if ((Z_OK != result) && (Z_STREAM_END != result))
      mxerror(boost::format(Y("Zlib decompression failed. Result: %1%\n")) % result);

  } while ((c_stream.avail_out == 0) && (result != Z_STREAM_END));

  dst->resize(c_stream.total_out);
  deflateEnd(&c_stream);

  mxverb(3, boost::format("zlib_compressor_c: Compression from %1% to %2%, %3%%%\n") % buffer->get_size() % dst->get_size() % (dst->get_size() * 100 / buffer->get_size()));

  return dst;
}

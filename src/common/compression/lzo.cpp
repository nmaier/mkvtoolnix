/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   lzo compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(HAVE_LZO)

# include "common/compression/lzo.h"

lzo_compressor_c::lzo_compressor_c()
  : compressor_c(COMPRESSION_LZO)
{
  int result;
  if ((result = lzo_init()) != LZO_E_OK)
    mxerror(boost::format(Y("lzo_init() failed. Result: %1%\n")) % result);

  wrkmem = (lzo_bytep)lzo_malloc(LZO1X_999_MEM_COMPRESS);
  if (nullptr == wrkmem)
    mxerror(Y("lzo_malloc(LZO1X_999_MEM_COMPRESS) failed.\n"));
}

lzo_compressor_c::~lzo_compressor_c() {
  safefree(wrkmem);
}

void
lzo_compressor_c::decompress(memory_cptr &) {
  mxerror(Y("lzo_compressor_c::decompress() not implemented\n"));
}

void
lzo_compressor_c::compress(memory_cptr &buffer) {
  int size             = buffer->get_size();
  unsigned char *dst   = (unsigned char *)safemalloc(size * 2);
  lzo_uint lzo_dstsize = size * 2;

  int result;
  if ((result = lzo1x_999_compress(buffer->get_buffer(), buffer->get_size(), dst, &lzo_dstsize, wrkmem)) != LZO_E_OK)
    mxerror(boost::format(Y("LZO compression failed. Result: %1%\n")) % result);

  int dstsize = lzo_dstsize;

  mxverb(3, boost::format("lzo_compressor_c: Compression from %1% to %2%, %3%%%\n") % size % dstsize % (dstsize * 100 / size));

  raw_size        += size;
  compressed_size += dstsize;
  items++;

  buffer = memory_cptr(new memory_c((unsigned char *)saferealloc(dst, dstsize), dstsize, true));
}

#endif  // HAVE_LZO

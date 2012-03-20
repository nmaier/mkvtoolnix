/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   zlib compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_COMPRESSION_ZLIB_H
#define MTX_COMMON_COMPRESSION_ZLIB_H

#include "common/common_pch.h"

#include <zlib.h>

#include "common/compression.h"

class zlib_compressor_c: public compressor_c {
public:
  zlib_compressor_c();
  virtual ~zlib_compressor_c();

protected:
  virtual memory_cptr do_decompress(memory_cptr const &buffer);
  virtual memory_cptr do_compress(memory_cptr const &buffer);
};

#endif // MTX_COMMON_COMPRESSION_ZLIB_H

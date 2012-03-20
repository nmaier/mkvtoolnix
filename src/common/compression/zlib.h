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

  virtual void decompress(memory_cptr &buffer);
  virtual void compress(memory_cptr &buffer);
};

#endif // MTX_COMMON_COMPRESSION_ZLIB_H

/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   bzlib compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_COMPRESSION_BZLIB_H
#define MTX_COMMON_COMPRESSION_BZLIB_H

#include "common/common_pch.h"

#if defined(HAVE_BZLIB_H)

# include <bzlib.h>

# include "common/compression.h"

class bzlib_compressor_c: public compressor_c {
public:
  bzlib_compressor_c();
  virtual ~bzlib_compressor_c();

  virtual memory_cptr do_decompress(memory_cptr const &buffer);
  virtual memory_cptr do_compress(memory_cptr const &buffer);
};
# endif // HAVE_BZLIB_H

#endif // MTX_COMMON_COMPRESSION_BZLIB_H

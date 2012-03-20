/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   lzo compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_COMPRESSION_LZO_H
#define MTX_COMMON_COMPRESSION_LZO_H

#include "common/common_pch.h"

#ifdef HAVE_LZO

# if defined(HAVE_LZO_LZO1X_H)
#  include <lzo/lzo1x.h>
#  include <lzo/lzoutil.h>
# else
#  include <lzoutil.h>
# endif

# include "common/compression.h"

class lzo_compressor_c: public compressor_c {
protected:
  lzo_byte *wrkmem;

public:
  lzo_compressor_c();
  virtual ~lzo_compressor_c();

  virtual memory_cptr do_decompress(memory_cptr const &buffer);
  virtual memory_cptr do_compress(memory_cptr const &buffer);
};

#endif // HAVE_LZO

#endif // MTX_COMMON_COMPRESSION_LZO_H

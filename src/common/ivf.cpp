/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for IVF data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <memory.h>

#include "common/fourcc.h"
#include "common/ivf.h"

namespace ivf {

file_header_t::file_header_t()
{
  memset(this, 0, sizeof(*this));
}

codec_e
file_header_t::get_codec()
  const {
  auto f = fourcc_c{fourcc};
  if (f.equiv("VP80"))
    return VP8;
  if (f.equiv("VP90"))
    return VP9;
  return UNKNOWN_CODEC;
}

frame_header_t::frame_header_t()
{
  memset(this, 0, sizeof(*this));
}

bool
is_keyframe(memory_cptr const &buffer,
            codec_e codec) {
  // Remember: bit numbers start with the least significant bit. Bit 0
  // == 1, bit 1 == 2 etc.

  if (!buffer || !buffer->get_size())
    return false;

  auto data = buffer->get_buffer();

  if (codec == VP8)
    return (data[0] & 0x01) == 0x00;

  // VP9
  // Bits 0, 1: frame marker, supposed to be 0x02
  // Bit 2:     version
  // Bit 3:     show existing frame directly flag
  // Bit 4:     frame type (0 means key frame)
  if ((data[0] & 0x04) == 0x04)
    return false;
  return (data[0] & 0x10) == 0x00;
}

};

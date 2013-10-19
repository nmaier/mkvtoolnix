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

#include "common/ivf.h"

namespace ivf {

file_header_t::file_header_t()
{
  memset(this, 0, sizeof(*this));
}

frame_header_t::frame_header_t()
{
  memset(this, 0, sizeof(*this));
}

bool
is_keyframe(memory_cptr const &buffer) {
  return buffer && (0 < buffer->get_size()) && ((buffer->get_buffer()[0] & 0x01) == 0);
}

};

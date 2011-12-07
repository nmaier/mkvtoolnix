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

ivf::file_header_t::file_header_t()
{
  memset(this, 0, sizeof(*this));
}

ivf::frame_header_t::frame_header_t()
{
  memset(this, 0, sizeof(*this));
}

bool
ivf::is_keyframe(const memory_cptr &buffer) {
  return buffer.is_set() && (0 < buffer->get_size()) && ((buffer->get_buffer()[0] & 0x01) == 0);
}

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

ivf_file_header_t::ivf_file_header_t()
{
  memset(this, 0, sizeof(*this));
}

ivf_frame_header_t::ivf_frame_header_t()
{
  memset(this, 0, sizeof(*this));
}

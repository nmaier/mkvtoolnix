/*
   mkvinfo -- info tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "info/mkvinfo.h"
#include "info/options.h"

options_c::options_c()
  : m_use_gui(false)
  , m_calc_checksums(false)
  , m_show_summary(false)
  , m_show_hexdump(false)
  , m_show_size(false)
  , m_hexdump_max_size(16)
{
}

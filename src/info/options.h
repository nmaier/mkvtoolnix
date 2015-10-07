/*
   mkvinfo -- info tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_INFO_OPTIONS_H
#define MTX_INFO_OPTIONS_H

#include "common/common_pch.h"

class options_c {
public:
  std::string m_file_name;
  bool m_use_gui, m_calc_checksums, m_show_summary, m_show_hexdump, m_show_size, m_show_track_info;
  int m_hexdump_max_size, m_verbose;
public:
  options_c();
};

#endif // MTX_INFO_OPTIONS_H

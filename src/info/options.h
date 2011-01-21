/*
   mkvinfo -- info tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __INFO_OPTIONS_H
#define __INFO_OPTIONS_H

#include "common/os.h"

#include <string>
#include <vector>

class options_c {
public:
  std::string m_file_name;
  bool m_use_gui, m_calc_checksums, m_show_summary, m_show_hexdump, m_show_size, m_show_track_info;
  int m_hexdump_max_size;
public:
  options_c();
};

#endif // __INFO_OPTIONS_H

/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __EXTRACT_OPTIONS_H
#define __EXTRACT_OPTIONS_H

#include "common/os.h"

#include <string>

class options_c {
public:
  enum extraction_mode_e {
    em_unknown,
    em_attachments,
    em_chapters,
    em_cuesheet,
    em_tags,
    em_timecodes_v2,
    em_tracks
  };

  std::string m_file_name;
  bool m_simple_chapter_format;
  bool m_parse_fully;
  extraction_mode_e m_extraction_mode;

public:
  options_c();
};

#endif // __EXTRACT_OPTIONS_H

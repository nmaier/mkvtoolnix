/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for WebM

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_WEBM_H
#define __MTX_COMMON_WEBM_H

#include "common/os.h"

#include <string>

bool is_webm_file_name(const std::string &file_name);

#endif // __MTX_COMMON_WEBM_H

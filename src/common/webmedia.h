/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for WebMedia

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_WEBMEDIA_H
#define __MTX_COMMON_WEBMEDIA_H

#include "common/os.h"

#include <string>

bool MTX_DLL_API is_webmedia_file_name(const std::string &file_name);

#endif // __MTX_COMMON_WEBMEDIA_H
